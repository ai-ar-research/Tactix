"""Python-owned DefaultPolicy startup windows, composed around root services.

The caller supplies structured preprocessing between the
windows, and keeps the Startup context until initial preprocessing is complete.
No method delegates to native DefaultPolicy or runs a search policy.
"""
import ctypes as C
import re
from . import capi, _QueryInterrupted

# C protocol identities, not a second set of strategy defaults.
_FLAT_PROFILES = ('parity_model', 'onehot_model', 'balanced_signature_model',
                  'fermat_model', 'lookup_circuit_model', 'simon_model',
                  'stconn_proof', 'signed_cardinality_proof', 'exact_cover_model',
                  'independent_set_model')


def _payload(text, tag):
    match = re.search(r':' + re.escape(tag) + r'\(([^()]*)\)', text)
    if match is None: raise ValueError(f'missing native {tag} parameters')
    return [int(value) for value in match[1].split(',')]


def _profile(kind, values):
    result = capi.tactix_online_parameters(
        kind=getattr(capi, 'TACTIX_ONLINE_PARAMETERS_' + kind.upper()))
    setattr(result.value, kind, getattr(capi, 'tactix_online_' + kind + '_parameters')(**values))
    return result


def lucky_profiles(definition):
    """Decode the installed native definition into owned C parameter records."""
    profiles = []
    for kind in _FLAT_PROFILES:
        fields = getattr(capi, 'tactix_online_' + kind + '_parameters')._fields_
        values = definition.section(kind)
        missing = {name for name, _ in fields} - values.keys()
        optional = {'refutation_maximum_variables', 'refutation_maximum_groups',
                    'refutation_maximum_constraints', 'refutation_maximum_arity',
                    'refutation_maximum_proof_additions'} if kind == 'onehot_model' else set()
        if missing - optional or values.keys() - {name for name, _ in fields} - {'enabled'}:
            raise ValueError(f'unsupported native {kind} fields')
        # Only these documented zero=default compatibility fields may be absent.
        profiles.append(_profile(kind, {name: values[name] for name, _ in fields if name in values}))
    values = _payload(definition['ramsey_startup.parameters'], 'ramsey-startup1')
    fields = capi.tactix_online_ramsey_startup_parameters._fields_
    if len(values) != len(fields): raise ValueError('unsupported native Ramsey profile')
    profiles.append(_profile('ramsey_startup', dict(zip((n for n, _ in fields), values))))

    values = _payload(definition['graph_labeling_model.parameters'], 'graph-labeling1')
    order = ('maximum_vertices', 'workspace_bytes', 'maximum_dfs_vertices', 'maximum_dfs_nodes',
             'maximum_dfs_starts', 'early_dfs_choices', 'repair_rounds', 'anneal_iterations',
             'stall_anneal_iterations', 'seed')
    if len(values) != len(order) + capi.TACTIX_GRAPH_STAGES:
        raise ValueError('unsupported native graph profile')
    fields = dict(zip(order, values))
    fields['work'] = (C.c_uint64 * capi.TACTIX_GRAPH_STAGES)(*values[len(order):])
    profiles.append(_profile('graph_labeling', fields))

    values = _payload(definition['affine_model.parameters'], 'affine1')
    fields = [name for name, _ in capi.tactix_online_affine_model_parameters._fields_][:18]
    if len(values) < 20: raise ValueError('incomplete native affine profile')
    scalars = dict(zip(fields, values))
    count = values[18]
    if count < 0 or len(values) < 20 + count: raise ValueError('incomplete affine projection')
    projection = (C.c_int * count)(*(int(v // 2 + 1) * (-1 if v & 1 else 1)
                                   for v in values[19:19+count]))
    scopes_count = values[19+count]
    tail = values[20+count:]
    if scopes_count < 0 or len(tail) != 4 * scopes_count:
        raise ValueError('incomplete affine scopes')
    scopes = (capi.tactix_online_affine_scope * scopes_count)(
        *(capi.tactix_online_affine_scope(*tail[i:i+4]) for i in range(0, len(tail), 4)))
    scalars.update(projection=projection, projection_count=count, scopes=scopes, scope_count=scopes_count)
    profiles.append(_profile('affine_model', scalars))
    return profiles


class DefaultStartup:
    """Independent ordered windows of native DefaultPolicy startup.

    Within the context: original_input() including affine root closure,
    constructors(), caller-owned structured preprocessing, early_lucky(). Keep
    the context open through the initial Probe/late-Lucky pipeline. The complete
    startup/inprocess/search coordinator is still being assembled. Profiles and
    native guidance publication remain installed on the execution view.
    """
    def __init__(self, execution):
        self.execution = execution
        self.definition = execution.default_definition
        if self.definition['inprocess.structured_root_residual_search_handoff']:
            raise NotImplementedError('residual guidance handoff must be supplied by the coordinator')
        self.result = 0
        self._stage = 'new'
        self._entered = False
        self._profiles = lucky_profiles(self.definition)

    def _support(self, name, argument=0):
        result = self.execution.c.tactix_support(getattr(capi, 'TACTIX_SUPPORT_' + name), argument)
        if result < 0: raise RuntimeError(f'startup support {name} refused')
        return result

    def _propagate(self):
        code = self.execution.c.tactix_propagate()
        if code < 0: raise RuntimeError('startup propagation refused')
        self._query_boundary()
        if code not in (0, 20): raise RuntimeError('startup propagation did not reach a root boundary')
        if code == 20: self.result = 20
        return code

    def _state(self):
        state = capi.tactix_execution_state()
        if self.execution.c.tactix_read_execution_state(C.byref(state)):
            raise RuntimeError('cannot observe execution state')
        return state

    def _query_boundary(self):
        if self.execution._session is not None and self.execution.next() in (
                capi.TACTIX_QUERY_RESTORE, capi.TACTIX_QUERY_INTERRUPTED, capi.TACTIX_QUERY_INVALID):
            raise _QueryInterrupted()

    def _window(self, expected, following):
        if (not self._entered and not self.result) or self._stage != expected:
            raise RuntimeError(f'startup expected {expected}, found {self._stage}')
        self._query_boundary()
        self._stage = following
        return not self.result

    def _lucky(self, instance, group=None):
        if self.result or (group and not self.definition[group + '.enabled']):
            return self.result
        self._query_boundary()
        eligible = self.execution.c.tactix_lucky_eligible(instance)
        if eligible < 0: raise RuntimeError('invalid Lucky observation boundary')
        if eligible:
            before = self.execution.rejected_models if self.execution._session is not None else 0
            code = self.execution.apply('Lucky', instance)
            self._query_boundary()
            if code in (10, 20): self.result = code
            elif self.execution._session is not None and self.execution.rejected_models > before:
                self._support('PREPARATION_RESTART')
        return self.result

    def __enter__(self):
        if self._stage != 'new': raise RuntimeError('startup cannot be reused')
        self.execution._check()
        self._query_boundary()
        state = self._state()
        if state.inprocess_phase_open:
            raise RuntimeError('Startup requires no pre-opened inprocessing phase')
        self._depth = state.context_depth + 1
        if self.execution.c.tactix_config(capi.TACTIX_CFG_GUIDANCE_PUBLICATION) != capi.TACTIX_GUIDANCE_NATIVE:
            if self.execution.c.tactix_set_guidance_publication(capi.TACTIX_GUIDANCE_NATIVE):
                raise RuntimeError('cannot select native startup guidance publication')
        # Complete the public input stream; an early SAT observation does not
        # replace the strategy's own opportunities or reconstructed-model checks.
        if self.execution.c.tactix_status() == 20:
            self.result, self._stage = 20, 'original'
            return self
        if self.execution.c.tactix_stat(capi.TACTIX_STAT_LEVEL):
            if self._support('PREPARATION_RESTART') == 20: self.result = 20
        self._support('RESET_GRAPH_LABELING_SESSION')
        for profile in self._profiles:
            if self.execution.c.tactix_install_parameters(self.execution._lib.rules['Lucky'], C.byref(profile)):
                raise ValueError('invalid native Lucky profile')
        self._support('ENTER_CONTEXT', capi.TACTIX_CONTEXT_STARTUP)
        self._entered, self._stage = True, 'original'
        return self

    def original_input(self):
        """Original-input certificates, root BCP and graph authentication."""
        if not self._window('original', 'constructors'): return self.result
        for instance, group in [(22, 'ramsey_startup'), (19, None), (20, 'startup_count')]:
            if self._lucky(instance, group): return self.result
        self._support('CLOSE_STARTUP_CERTIFICATE')
        if self._propagate(): return self.result
        self._lucky(26, 'graph_labeling_model')
        state = capi.tactix_graph_labeling_state()
        if self.execution.c.tactix_read_graph_labeling_state(C.byref(state)):
            raise RuntimeError('cannot observe graph authentication')
        if state.authentication_status == capi.TACTIX_GRAPH_AUTH_INTERRUPTED:
            raise _QueryInterrupted()
        if not self.result: self._affine_root_closure()
        return self.result

    def _affine_root_closure(self):
        config = self.definition.section('affine_root_closure')
        state = self._state()
        clauses = self.execution.c.tactix_stat(capi.TACTIX_STAT_IRREDUNDANT_CLAUSES)
        extent = state.external_variables
        additions = config['maximum_proof_additions']
        if (not config['enabled'] or not state.provenance_bound or not state.affine_matrix_bound
                or not clauses or clauses > min(config['maximum_clauses'], 2**32-1, (2**64-1)//3)
                or extent > 2**31-1 or additions > 2**31-1-extent):
            return
        basis = capi.tactix_online_affine_consequence_parameters(
            maximum_clauses=clauses, maximum_snapshot_literals=3*clauses,
            maximum_snapshot_work=config['maximum_snapshot_work'], maximum_groups=clauses,
            maximum_patterns=clauses, maximum_elimination_work=config['maximum_recognition_work'],
            maximum_arity=3, maximum_intermediate_width=config['maximum_row_width'],
            maximum_sources=clauses, maximum_output_width=4,
            maximum_proof_additions=additions, maximum_proof_deletions=config['maximum_proof_deletions'],
            maximum_journal_bytes=config['maximum_journal_bytes'],
            semantics=capi.TACTIX_ONLINE_AFFINE_CONSEQUENCE_CERTIFIED_BASIS_V2,
            basis_maximum_work=config['maximum_basis_work'],
            basis_maximum_intermediate_width=config['maximum_row_width'],
            basis_maximum_sources=clauses, basis_maximum_premises=clauses, basis_maximum_nodes=clauses,
            basis_maximum_direct_width=3, maximum_external_extent=extent+additions,
            maximum_session_keys=clauses, maximum_session_work=config['maximum_session_work'])
        parameters = capi.tactix_online_parameters(kind=capi.TACTIX_ONLINE_PARAMETERS_AFFINE_CONSEQUENCE)
        parameters.value.affine_consequence = basis
        rule = self.execution._lib.rules['AffineMatrixInitialize']
        if self.execution.c.tactix_install_parameters(rule, C.byref(parameters)):
            return  # Native rejects an invalid derived profile before any action.
        initialized_ok = refuted = False
        def advance(operation):
            nonlocal initialized_ok, refuted
            profile = capi.tactix_online_affine_matrix_initialize_parameters(
                basis=basis, admission=capi.TACTIX_AFFINE_MATRIX_COMPLETE_SNAPSHOT, operation=operation)
            if self.execution.c.tactix_install_affine_matrix_profile(C.byref(profile)):
                raise ValueError('invalid affine root operation')
            try:
                self._query_boundary()
                self.execution.apply('AffineMatrixInitialize')
            finally:
                # Native installs this basis permanently, then restores it
                # after each temporary operation profile.
                if self.execution.c.tactix_install_parameters(rule, C.byref(parameters)):
                    raise RuntimeError('cannot restore affine basis')
            result = capi.tactix_affine_matrix_result()
            if self.execution.c.tactix_read_affine_matrix_result(C.byref(result)):
                raise RuntimeError('cannot observe affine root operation')
            if operation == capi.TACTIX_AFFINE_MATRIX_INITIALIZE:
                initialized_ok = result.initialization_status == capi.TACTIX_AFFINE_INITIALIZATION_APPLIED
            else:
                refuted = (result.service_status == capi.TACTIX_AFFINE_SERVICE_APPLIED
                           and result.service_result == capi.TACTIX_AFFINE_RESULT_UNSAT)
            self._query_boundary()
            return result
        try:
            advance(capi.TACTIX_AFFINE_MATRIX_INITIALIZE)
            if not initialized_ok: return
            while True:
                result = advance(capi.TACTIX_AFFINE_MATRIX_SEARCH_STEP)
                if result.service_status != capi.TACTIX_AFFINE_SERVICE_APPLIED: return
                if refuted:
                    self.result = 20
                    return
                if not result.bcp_pending: raise RuntimeError('affine event without pending BCP')
                if self._propagate():
                    refuted = True
                    return
        finally:
            if initialized_ok and not refuted:
                self._support('RELEASE_AFFINE_MATRIX', capi.TACTIX_AFFINE_MATRIX_RELEASE_DETACHED)

    def constructors(self):
        """Run after original-input/root closure, before structured work."""
        if not self._window('constructors', 'early'): return self.result
        for instance, group in [(16, 'signed_cardinality_proof'), (17, 'exact_cover_model'),
                (18, 'independent_set_model'), (15, 'stconn_proof'),
                (10, 'balanced_signature_model'), (11, 'fermat_model'), (12, 'cerny_model'),
                (13, 'lookup_circuit_model'), (14, 'simon_model'), (23, None),
                (6, 'parity_model'), (24, None)]:
            if self._lucky(instance, group): break
        return self.result

    def early_lucky(self):
        """Run after structured preprocessing and before H materialization."""
        if not self._window('early', 'preprocess'): return self.result
        if self._lucky(9, 'onehot_model'): return self.result
        if self._state().input_order_captured:
            self._support('RELEASE_INPUT_ORDER')
        if self._lucky(capi.TACTIX_INSTANCE_CANONICAL): return self.result
        return self._lucky(7, 'onehot_model')

    def attach_guidance(self):
        if not self._window('preprocess', 'attached'): return self.result
        self._support('MATERIALIZE_COLD_STATE')
        self._support('ATTACH_HEURISTICS')
        return self.result

    def close(self):
        if self._entered:
            state = self._state()
            if (state.context_depth != self._depth or state.context_top != capi.TACTIX_CONTEXT_STARTUP
                    or state.inprocess_phase_open):
                raise RuntimeError('close caller-owned phases/contexts before Startup')
            self._support('LEAVE_CONTEXT')
            self._entered = False
        self._stage = 'closed'

    def __exit__(self, kind, error, trace):
        try: self.close()
        except BaseException as cleanup:
            if error is None: raise
            error.startup_cleanup_error = cleanup
