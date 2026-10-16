# Regenerate the paper's evaluation from the archived logs.
#
#   make            data, figures and the printed report
#   make check      regenerate data/ from logs/ and compare with the committed copy
#   make LOGS=DIR   use the results of a new run (see run/README.md) instead

PYTHON ?= python3
LOGS ?= logs
DATA ?= data
FIGURES ?= figures
export MPLCONFIGDIR ?= /tmp/tactix-matplotlib

PLOT = $(PYTHON) scripts/plot_final_results.py --data $(DATA)/tierd-times.csv

FIGURE_FILES = $(FIGURES)/tierd-cactus.pdf $(FIGURES)/tierd-scatter.pdf \
	$(FIGURES)/tierd-scatter-satsuma.pdf $(FIGURES)/tierd-scatter-lympho.pdf \
	$(FIGURES)/interface-overhead.pdf $(FIGURES)/milestone-ladder.pdf \
	$(FIGURES)/development-roadmap.pdf $(FIGURES)/chvatal-scatter.pdf \
	$(FIGURES)/chvatal-table.tex

.PHONY: all normalize data figures report check clean

all: data figures report

normalize:
	$(PYTHON) scripts/normalize.py

data:
	$(PYTHON) scripts/make_data.py --logs $(LOGS) --out $(DATA)

figures: $(FIGURE_FILES)

report:
	$(PYTHON) scripts/report.py --data $(DATA)

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
	$(PYTHON) scripts/plot_chvatal.py --data $< --output $@
$(FIGURES)/chvatal-table.tex: $(DATA)/chvatal-results.csv
	$(PYTHON) scripts/table_chvatal.py --data $< --output $@

check:
	rm -rf .check && mkdir -p .check/logs .check/data
	$(PYTHON) scripts/normalize.py --out .check/logs
	$(PYTHON) scripts/make_data.py --logs .check/logs --out .check/data
	@status=0; for f in .check/data/*.csv; do \
	  if cmp -s $$f $(DATA)/$$(basename $$f); then echo "identical  $$(basename $$f)"; \
	  else echo "DIFFERENT  $$(basename $$f)"; status=1; fi; done; \
	rm -rf .check; exit $$status

clean:
	rm -rf $(FIGURES) .check
