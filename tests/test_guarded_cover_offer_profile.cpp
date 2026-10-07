#undef NDEBUG
#include "strategy/guarded_cover_offer_profile.hpp"
#include <cassert>
#include <string>

int main() {
  using namespace tactix;
  using namespace tactix::strategy;
  const std::string good = "guarded-cover-offer1(100,1000,10,10,10,10,10,10,10000,20,30,40,50,60,70,100000,100,1000,100000,80,90,10,8,1000,2000)";
  DirectCspGuardedCoverOfferLimits limits;
  std::string error;
  assert(parse_guarded_cover_offer_limits(good + "\r\n", limits, &error));
  assert(error.empty() && limits.valid());
  assert(limits.theory.maximum_domain_values == 10);
  assert(limits.proposals.maximum_nominations == 70);
  assert(limits.maximum_binary_edges == 2000);
  assert(serialize_guarded_cover_offer_limits(limits) == good);
  auto reject = [&](const std::string &bad) {
    assert(!parse_guarded_cover_offer_limits(bad, limits, &error));
    assert(!error.empty() && serialize_guarded_cover_offer_limits(limits) == good);
  };
  reject(""); reject("guarded-cover-offer1()"); reject(good + "garbage");
  reject(" " + good); reject(good.substr(0, good.size() - 1));
  auto bad = good; bad.replace(bad.find("100,"), 3, "0"); reject(bad);
  bad = good; bad.replace(bad.find("100,"), 3, "-1"); reject(bad);
  bad = good; bad.replace(bad.find("100,"), 3, "+1"); reject(bad);
  bad = good; bad.replace(bad.find("100,"), 3, "18446744073709551616"); reject(bad);
  bad = good; bad.replace(bad.find(",10,10,"), 3, ",4294967296"); reject(bad);
  bad = good; bad.insert(bad.size() - 1, ",1"); reject(bad);
  bad = good; bad.erase(bad.rfind(','), bad.size() - 1 - bad.rfind(',')); reject(bad);
  bad = good; bad.insert(bad.find(',') + 1, " "); reject(bad);
  return 0;
}
