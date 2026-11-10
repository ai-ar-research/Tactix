// Native field, public C field, explicit fixture value (not DefaultPolicy data).
SR_FIELD(producer, producer, 3) // GraphSymmetry
SR_FIELD(maximum_validation_ticks, maximum_validation_ticks, 1024)
SR_FIELD(maximum_detection_ticks, maximum_detection_ticks, 100000)
SR_FIELD(graph_symmetry.maximum_graph_vertices, maximum_graph_vertices, 1024)
SR_FIELD(graph_symmetry.maximum_graph_edges, maximum_graph_edges, 4096)
SR_FIELD(graph_symmetry.maximum_generators, maximum_symmetry_generators, 32)
SR_FIELD(graph_symmetry.backend_search_budget, symmetry_backend_search_budget, 10000)
SR_FIELD(graph_symmetry.backend_support_budget, symmetry_backend_support_budget, 100000)
SR_FIELD(graph_symmetry.backend_component_size_limit, symmetry_backend_component_size_limit, 1024)
SR_FIELD(graph_symmetry.maximum_fixing_instances, maximum_symmetry_fixing_instances, 64)
SR_FIELD(graph_symmetry.maximum_generator_instances_per_pass, maximum_symmetry_generator_instances_per_pass, 64)
SR_FIELD(graph_symmetry.maximum_group_ticks, maximum_symmetry_group_ticks, 100000)
SR_FIELD(graph_symmetry.fixing_mode, symmetry_fixing_mode, 2) // StabilizerThenGenerators
