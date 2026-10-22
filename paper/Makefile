# Regenerate the paper's evaluation from the archived logs.
#
#   make            data, figures and the printed report
#   make check      regenerate data/ from logs/ and compare with the committed copy
#   make LOGS=DIR   use the results of a new run instead (bench.py's output)
#   make compare RUN=DIR OUT=DIR
#                   a rerun next to the paper's logs on the same runs (compare.py)

PYTHON ?= python3
LOGS ?= logs
DATA ?= data
FIGURES ?= figures
# Scratch space for `make check` (point it elsewhere in a read-only image).
CHECK ?= .check
export MPLCONFIGDIR ?= /tmp/tactix-matplotlib

# The run's limits (written by make_data.py) set the figures' axes.
LIMITS = --limits $(DATA)/limits.json
PLOT = $(PYTHON) scripts/plot_final_results.py --data $(DATA)/tierd-times.csv $(LIMITS)

FIGURE_FILES = $(FIGURES)/tierd-cactus.pdf $(FIGURES)/tierd-scatter.pdf \
	$(FIGURES)/tierd-scatter-satsuma.pdf $(FIGURES)/tierd-scatter-lympho.pdf \
	$(FIGURES)/interface-overhead.pdf $(FIGURES)/milestone-ladder.pdf \
	$(FIGURES)/development-roadmap.pdf $(FIGURES)/chvatal-scatter.pdf \
	$(FIGURES)/chvatal-table.tex

.PHONY: all normalize data figures report check clean compare activity

all: data figures report

normalize:
	$(PYTHON) scripts/normalize.py

data:
	$(PYTHON) scripts/make_data.py --logs $(LOGS) --out $(DATA)

figures: $(FIGURE_FILES)

report:
	$(PYTHON) scripts/report.py --data $(DATA)

compare:
	$(PYTHON) scripts/compare.py --run $(RUN) --out $(OUT)

# Table 6's activity columns, recomputed from the git history.
activity:
	$(PYTHON) scripts/analyze_milestone_activity.py --output-dir $(DATA)

$(FIGURES)/tierd-cactus.pdf: $(DATA)/tierd-times.csv
	$(PLOT) --cactus $@
$(FIGURES)/tierd-scatter.pdf: $(DATA)/tierd-times.csv
	$(PLOT) --scatter $@
$(FIGURES)/tierd-scatter-satsuma.pdf: $(DATA)/tierd-times.csv
	$(PLOT) --scatter-satsuma $@
$(FIGURES)/tierd-scatter-lympho.pdf: $(DATA)/tierd-times.csv
	$(PLOT) --scatter-lympho $@
$(FIGURES)/interface-overhead.pdf: $(DATA)/interface-overhead.csv
	$(PLOT) --interface-data $< --interface $@
$(FIGURES)/milestone-ladder.pdf: $(DATA)/milestone-times.csv
	$(PLOT) --milestone-data $< --milestones $@
# Development activity comes from git history (scripts/analyze_milestone_activity.py),
# not from solver runs, so it is always read from data/.
$(FIGURES)/development-roadmap.pdf: data/milestone-activity.csv
	$(PYTHON) scripts/plot_development_roadmap.py --data $< --output $@
$(FIGURES)/chvatal-scatter.pdf: $(DATA)/chvatal-results.csv
	$(PYTHON) scripts/plot_chvatal.py --data $< --output $@ $(LIMITS)
$(FIGURES)/chvatal-table.tex: $(DATA)/chvatal-results.csv
	$(PYTHON) scripts/table_chvatal.py --data $< --output $@ $(LIMITS) >/dev/null

check:
	rm -rf $(CHECK) && mkdir -p $(CHECK)/logs $(CHECK)/data
	$(PYTHON) scripts/normalize.py --out $(CHECK)/logs
	$(PYTHON) scripts/make_data.py --logs $(CHECK)/logs --out $(CHECK)/data
	@status=0; for f in $(CHECK)/data/*.csv; do \
	  if cmp -s $$f $(DATA)/$$(basename $$f); then echo "identical  $$(basename $$f)"; \
	  else echo "DIFFERENT  $$(basename $$f)"; status=1; fi; done; \
	rm -rf $(CHECK); exit $$status

clean:
	rm -rf $(FIGURES) $(CHECK)
