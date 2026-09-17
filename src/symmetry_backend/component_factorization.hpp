#pragma once

// Private to the exception-enabled backend. The public producer remains the
// sole owner of exact CNF validation and rule publication.
namespace tactix::symmetry_backend::detail {

struct ComponentBudgetExhausted {};

struct ComponentFixings {
  Fixings value;
  ~ComponentFixings() { release(value); }
};

inline Status fix_support_components(const GroupInput &input,
                                     const GroupSettings &settings,
                                     StopCallback stop, void *context,
                                     Fixings &result) {
  struct Component {
    std::vector<uint32_t> points;
    std::vector<uint64_t> generators;
    std::vector<uint64_t> offsets{0};
    std::vector<uint32_t> literals;
    std::vector<uint8_t> amos;
  };
  const uint32_t domain = input.domain_size;
  uint64_t ticks = 0;
  const auto poll = [&] {
    if (stop && stop(context)) throw dejavu::termination_requested();
  };
  const auto charge = [&](uint64_t amount) {
    if (amount > settings.maximum_ticks - ticks) {
      ticks = settings.maximum_ticks;
      throw ComponentBudgetExhausted{};
    }
    ticks += amount;
  };
  std::vector<uint8_t> kinds;
  std::vector<uint32_t> levels, representatives, others, images, image_points;
  std::vector<uint64_t> image_offsets{0};
  bool incomplete = false;
  try {
    std::vector<uint32_t> parent(domain), sizes(domain, 1);
    for (uint32_t p = 0; p < domain; ++p) {
      if (!(p & 1023u)) poll();
      charge(1);
      parent[p] = p;
    }
    const auto root = [&](uint32_t p) {
      while (parent[p] != p) {
        parent[p] = parent[parent[p]];
        p = parent[p];
      }
      return p;
    };
    const auto join = [&](uint32_t a, uint32_t b) {
      a = root(a);
      b = root(b);
      if (a == b) return;
      if (sizes[a] < sizes[b]) std::swap(a, b);
      parent[b] = a;
      sizes[a] += sizes[b];
    };
    std::vector<uint32_t> first(input.generator_count, UINT32_MAX);
    for (uint64_t g = 0; g < input.generator_count; ++g) {
      poll();
      // Algebraically retain every old identity inspection's work charge;
      // incomplete private setup publishes no component state.
      charge(domain);
      visit_generator_moves(input, g, poll, [&](uint32_t p, uint32_t) {
        if (first[g] == UINT32_MAX) first[g] = p;
        // The whole support stays coupled, not individual permutation cycles.
        join(first[g], p);
        if (!(domain & 1u)) join(p, p ^ 1u);
      });
    }
    std::vector<Component> components;
    std::vector<uint32_t> component(domain, UINT32_MAX);
    for (uint64_t g = 0; g < input.generator_count; ++g) {
      poll();
      charge(1);
      if (first[g] == UINT32_MAX) continue;
      const uint32_t r = root(first[g]);
      if (component[r] == UINT32_MAX) {
        component[r] = static_cast<uint32_t>(components.size());
        components.emplace_back();
      }
      components[component[r]].generators.push_back(g);
    }
    GroupSettings child_settings = settings;
    child_settings.factor_support_components = false;
    SymmetryTrace{}.event("group_components", "count\t%zu\tdomain\t%u\tticks\t%llu",
                         components.size(), domain,
                         static_cast<unsigned long long>(ticks));
    if (components.size() <= 1) {
      // Preserve all point/clause identities and ordering for connected input.
      child_settings.maximum_ticks -= ticks;
      if (settings.chain_mode && settings.planning_tick_limit) {
        if (ticks >= settings.planning_tick_limit)
          throw ComponentBudgetExhausted{};
        child_settings.planning_tick_limit -= ticks;
      }
      const Status status = fix_clauses(input, child_settings, stop, context, result);
      result.ticks += ticks;
      return status;
    }
    std::vector<uint32_t> local(domain, UINT32_MAX);
    // Read component roots into a separate array: overwriting root labels
    // during this scan could otherwise change later point classification.
    const auto root_components = component;
    for (uint32_t p = 0; p < domain; ++p) {
      if (!(p & 1023u)) poll();
      charge(1);
      component[p] = root_components[root(p)];
      if (component[p] == UINT32_MAX) continue;
      auto &points = components[component[p]].points;
      local[p] = static_cast<uint32_t>(points.size());
      points.push_back(p);
    }
    for (uint64_t c = 0; c < input.clause_count; ++c) {
      poll();
      charge(1);
      uint32_t owner = UINT32_MAX;
      bool omit = false;
      for (uint64_t at = input.clause_offsets[c];
           at < input.clause_offsets[c + 1]; ++at) {
        if (!(at & 1023u)) poll();
        charge(1);
        const uint32_t p = input.literals[at];
        if (input.values[p] > 0) { omit = true; break; }
        if (input.values[p] < 0) continue;
        if (component[p] == UINT32_MAX ||
            (owner != UINT32_MAX && owner != component[p])) {
          omit = true;
          break;
        }
        owner = component[p];
      }
      // Unknown outside literals cannot be projected out of a source clause.
      if (omit || owner == UINT32_MAX) continue;
      auto &part = components[owner];
      for (uint64_t at = input.clause_offsets[c];
           at < input.clause_offsets[c + 1]; ++at) {
        if (!(at & 1023u)) poll();
        charge(1);
        const uint32_t p = input.literals[at];
        if (component[p] == owner) part.literals.push_back(local[p]);
      }
      part.offsets.push_back(part.literals.size());
      part.amos.push_back(input.clause_is_amo ? input.clause_is_amo[c] : 0);
    }
    uint64_t level_offset = 0;
    for (const auto &part : components) {
      poll();
      if (kinds.size() >= settings.maximum_instances) {
        incomplete = true;
        break;
      }
      const uint32_t n = static_cast<uint32_t>(part.points.size());
      if (!n) throw std::logic_error("empty support component");
      std::vector<uint32_t> generators;
      std::vector<int8_t> values;
      for (const uint32_t p : part.points) {
        charge(1);
        values.push_back(input.values[p]);
      }
      for (const uint64_t g : part.generators) {
        poll();
        // Both lists are ascending. Merge once instead of binary-searching
        // the same sparse generator for every component point.
        uint64_t at = input.generator_offsets ? input.generator_offsets[g] : 0;
        const uint64_t end = input.generator_offsets ? input.generator_offsets[g + 1] : 0;
        for (const uint32_t p : part.points) {
          if (!(local[p] & 1023u)) poll();
          charge(1);
          uint32_t image = p;
          if (input.generator_offsets) {
            while (at < end && input.generator_points[at] < p) ++at;
            if (at < end && input.generator_points[at] == p)
              image = input.generator_images[at];
          } else image = input.generator_images[g * input.domain_size + p];
          generators.push_back(local[image]);
        }
      }
      child_settings.maximum_instances = settings.maximum_instances - kinds.size();
      child_settings.maximum_ticks = settings.maximum_ticks - ticks;
      if (settings.chain_mode && settings.planning_tick_limit) {
        if (ticks >= settings.planning_tick_limit) {
          incomplete = true;
          break;
        }
        child_settings.planning_tick_limit = settings.planning_tick_limit - ticks;
      }
      const GroupInput child{n, part.generators.size(), generators.data(),
          part.amos.size(), part.offsets.data(), part.literals.size(),
          part.literals.data(), values.data(), part.amos.data()};
      ComponentFixings owned;
      const Status status = fix_clauses(child, child_settings, stop, context, owned.value);
      const auto &fixed = owned.value;
      charge(fixed.ticks);
      if (status != Status::Complete && status != Status::Incomplete) {
        result.ticks = ticks;
        result.status = status;
        return status;
      }
      if (fixed.image_offsets || (fixed.count &&
          (!fixed.images || !fixed.levels || !fixed.kinds ||
           !fixed.representatives || !fixed.others || fixed.domain_size != n)))
        throw std::logic_error("invalid dense component result");
      if (fixed.count > SIZE_MAX / n / sizeof(uint32_t) ||
          images.size() > SIZE_MAX / sizeof(uint32_t) - fixed.count * n)
        throw std::bad_alloc();
      // Precharge the whole component before appending its first row: budget
      // exhaustion must retain only complete components. Per image cell:
      // one local inspection, two staged source/target writes and two export
      // writes (identity cells need fewer). No global identity rows are built.
      const uint64_t cells = fixed.count * n;
      constexpr uint64_t cell_work = 1 + 2 + 2;
      if (cells > UINT64_MAX / cell_work) throw std::bad_alloc();
      charge(cell_work * cells);
      uint64_t next_level = level_offset;
      for (uint64_t row = 0; row < fixed.count; ++row) {
        poll();
        if (level_offset + fixed.levels[row] > UINT32_MAX) throw std::bad_alloc();
        kinds.push_back(fixed.kinds[row]);
        levels.push_back(static_cast<uint32_t>(level_offset + fixed.levels[row]));
        next_level = std::max(next_level, static_cast<uint64_t>(levels.back()) + 1);
        if (fixed.representatives[row] >= n ||
            (fixed.others[row] != UINT32_MAX && fixed.others[row] >= n))
          throw std::logic_error("invalid component orbit point");
        representatives.push_back(part.points[fixed.representatives[row]]);
        others.push_back(fixed.others[row] == UINT32_MAX ? UINT32_MAX
                                                       : part.points[fixed.others[row]]);
        for (uint32_t p = 0; p < n; ++p) {
          if (!(p & 1023u)) poll();
          const uint32_t target = fixed.images[row * n + p];
          if (target >= n) throw std::logic_error("invalid component image");
          if (target == p) continue;
          image_points.push_back(part.points[p]);
          images.push_back(part.points[target]);
        }
        image_offsets.push_back(images.size());
      }
      level_offset = next_level;
      if (status == Status::Incomplete) { incomplete = true; break; }
    }
  } catch (const ComponentBudgetExhausted &) {
    incomplete = true;
  }
  poll();
  if (!kinds.empty()) {
    const size_t count = kinds.size();
    if (count > SIZE_MAX / sizeof(uint32_t)) throw std::bad_alloc();
    result.kinds = static_cast<uint8_t *>(std::malloc(count));
    result.levels = static_cast<uint32_t *>(std::malloc(count * sizeof(uint32_t)));
    result.representatives = static_cast<uint32_t *>(std::malloc(count * sizeof(uint32_t)));
    result.others = static_cast<uint32_t *>(std::malloc(count * sizeof(uint32_t)));
    result.images = static_cast<uint32_t *>(std::malloc(images.size() * sizeof(uint32_t)));
    result.image_points = static_cast<uint32_t *>(std::malloc(images.size() * sizeof(uint32_t)));
    if (image_offsets.size() > SIZE_MAX / sizeof(uint64_t)) throw std::bad_alloc();
    result.image_offsets = static_cast<uint64_t *>(std::malloc(image_offsets.size() * sizeof(uint64_t)));
    if (!result.kinds || !result.levels || !result.representatives ||
        !result.others || !result.image_offsets ||
        (!images.empty() && (!result.images || !result.image_points))) throw std::bad_alloc();
    for (size_t row = 0; row < count; ++row) {
      if (!(row & 1023u)) poll();
      result.kinds[row] = kinds[row];
      result.levels[row] = levels[row];
      result.representatives[row] = representatives[row];
      result.others[row] = others[row];
    }
    for (size_t p = 0; p < images.size(); ++p) {
      if (!(p & 1023u)) poll();
      result.images[p] = images[p];
      result.image_points[p] = image_points[p];
    }
    assert(image_offsets.size() == count + 1);
    for (size_t row = 0; row < image_offsets.size(); ++row) {
      if (!(row & 1023u)) poll();
      result.image_offsets[row] = image_offsets[row];
    }
  }
  result.image_entries = images.size();
  result.domain_size = domain;
  result.count = kinds.size();
  result.ticks = ticks;
  result.status = incomplete ? Status::Incomplete : Status::Complete;
  return result.status;
}
} // namespace tactix::symmetry_backend::detail
