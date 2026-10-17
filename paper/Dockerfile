# The Tactix paper artifact: solvers, proof checkers and analysis in one image.
#
#   docker build -f paper/Dockerfile -t tactix-artifact .
#   docker run --rm -it tactix-artifact
#
# Build from the repository root, in a clone that has the paper-commits
# branch. Building compiles Tactix at every commit the paper evaluated, the
# reference solvers and the proof checkers (about half an hour), and
# downloads the instances of the reduced experiment set, so the smoke test and
# the reduced evaluation then run offline.
FROM ubuntu:24.04
COPY paper/install-deps.sh /tmp/install-deps.sh
RUN /tmp/install-deps.sh
ENV PATH=/root/.elan/bin:$PATH TAR_OPTIONS=--no-same-owner
WORKDIR /tactix
COPY . /tactix
RUN paper/run/setup.sh all \
 && for e in tierd milestones interface cruxmiter chvatal; do \
        paper/run/bench.py fetch $e --subset short; done \
 && paper/run/bench.py fetch smoke
CMD ["/bin/bash"]
