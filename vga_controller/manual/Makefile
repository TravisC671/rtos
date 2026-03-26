# Makefile for VGA Controller User Manual

# LaTeX compiler
LATEX = pdflatex
BIBTEX = bibtex

# Main document
MAIN = vga_manual

# Output directory
OUTDIR = .

.PHONY: all clean view

all: $(MAIN).pdf

$(MAIN).pdf: $(MAIN).tex
	$(LATEX) -output-directory=$(OUTDIR) $(MAIN).tex
	$(LATEX) -output-directory=$(OUTDIR) $(MAIN).tex
	$(LATEX) -output-directory=$(OUTDIR) $(MAIN).tex

view: $(MAIN).pdf
	@if command -v evince >/dev/null 2>&1; then \
		evince $(MAIN).pdf & \
	elif command -v xdg-open >/dev/null 2>&1; then \
		xdg-open $(MAIN).pdf & \
	else \
		echo "No PDF viewer found. Please open $(MAIN).pdf manually."; \
	fi

clean:
	rm -f $(MAIN).aux $(MAIN).log $(MAIN).out $(MAIN).toc
	rm -f $(MAIN).lof $(MAIN).lot $(MAIN).bbl $(MAIN).blg
	rm -f $(MAIN).fdb_latexmk $(MAIN).fls $(MAIN).synctex.gz

distclean: clean
	rm -f $(MAIN).pdf

# Help target
help:
	@echo "VGA Controller Manual Build System"
	@echo ""
	@echo "Targets:"
	@echo "  all       - Build the PDF manual (default)"
	@echo "  view      - Build and open the PDF"
	@echo "  clean     - Remove auxiliary files"
	@echo "  distclean - Remove all generated files including PDF"
	@echo "  help      - Show this help message"
	@echo ""
	@echo "Requirements:"
	@echo "  - pdflatex (TeX Live or MiKTeX)"
	@echo "  - Standard LaTeX packages (see vga_manual.tex preamble)"
