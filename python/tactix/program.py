"""Owned authored-program snapshots; callers choose scheduling and execution."""
import ctypes as C
from . import capi


class _StageStorage:
    def __init__(self, library, handle):
        self.library, self.handle = library, handle

    def __del__(self):
        self.library.dll.tactix_default_stage_snapshot_release(self.handle)


class _StorageBuffer(bytearray):
    pass


def _leased_copy(value, storage):
    # ctypes propagates its buffer owner through _objects when fields or
    # records are copied. Plain Python attributes on a record do not propagate.
    buffer = _StorageBuffer(C.sizeof(value))
    buffer.snapshot_storage = storage
    result = type(value).from_buffer(buffer)
    C.memmove(C.addressof(result), C.addressof(value), C.sizeof(value))
    return result


class DefaultStage:
    """One resolved native structured-stage definition, without running it.

    Returned ctypes records retain their nested pointer storage, including
    after close(). Executors deep-copy their input. The snapshot and returned
    records can outlive the originating Solver or borrowed Execution view.
    """
    def __init__(self, execution, family, index):
        execution._check()
        if not isinstance(index, int) or not 0 <= index < 2**64:
            raise ValueError('stage index must be a uint64')
        if family not in (capi.TACTIX_DEFAULT_STAGE_REGULAR,
                          capi.TACTIX_DEFAULT_STAGE_BOUNDED_BACKTRACKING,
                          capi.TACTIX_DEFAULT_STAGE_LARGE_DOMAIN):
            raise ValueError('unknown structured-stage family')
        self._lib = execution._lib
        handle = self._lib.dll.tactix_default_stage_snapshot_create(
            execution._view, family, index)
        if not handle:
            raise ValueError('missing or unrepresentable native structured stage')
        self._storage = _StageStorage(self._lib, handle)

    def _definition(self):
        storage = getattr(self, '_storage', None)
        if not storage:
            raise RuntimeError('stage snapshot has been released')
        definition = self._lib.dll.tactix_default_stage_snapshot_read(storage.handle).contents
        definition._snapshot_storage = storage
        return definition

    @property
    def program(self):
        """Copied v2 program record retaining its read-only nested storage."""
        d = self._definition()
        return _leased_copy(d.program, d._snapshot_storage)

    @property
    def on_budget_exhaustion(self):
        return self._definition().on_budget_exhaustion

    @property
    def progress_followups(self):
        d = self._definition()
        return tuple(d.progress_followups[:d.progress_followup_count])

    @property
    def before_progress(self):
        d = self._definition()
        return tuple(d.before_progress[:d.before_progress_count])

    @property
    def structured_handoff_after_prior_progress(self):
        return bool(self._definition().structured_handoff_after_prior_progress)

    def steps_after_feedback(self, prior_progress=False, unique_literal_amo=False,
                             nonforcing_cut=False):
        """Copy step records and apply native inter-stage scheduling feedback.

        Returned records retain nested storage. No step is selected or run.
        All parameter values start from the resolved native authored program.
        """
        d = self._definition()
        steps = (capi.tactix_online_step_v2 * d.program.step_count)(
            *(capi.tactix_online_step_v2.from_buffer_copy(step)
              for step in d.program.steps[:d.program.step_count]))
        steps = _leased_copy(steps, d._snapshot_storage)
        for step in steps:
            p = step.base.parameters
            if p.kind != capi.TACTIX_ONLINE_PARAMETERS_SUBSTITUTION_REDUNDANCY:
                continue
            sr = p.value.substitution_redundancy
            if sr.producer != capi.TACTIX_ONLINE_SR_GRAPH_SYMMETRY:
                continue
            if unique_literal_amo or nonforcing_cut:
                sr.symmetry_nonforcing_cut_admission = capi.TACTIX_ONLINE_SYMMETRY_CUT_ADMISSION_PUBLISH
            if d.structured_handoff_after_prior_progress and not prior_progress:
                sr.symmetry_use_structured_generators_mode = capi.TACTIX_ONLINE_SETTING_DISABLE
                sr.symmetry_stabilize_structured_variables_mode = capi.TACTIX_ONLINE_SETTING_DISABLE
        return steps

    def close(self):
        # Existing returned records hold independent leases. Closing refuses
        # new reads; native storage is released only after the last lease.
        self._storage = None

    def __enter__(self):
        self._definition()
        return self

    def __exit__(self, *_):
        self.close()

    def __del__(self):
        self.close()
