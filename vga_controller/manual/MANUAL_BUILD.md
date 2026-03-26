# Building the VGA Controller User Manual

This directory contains a comprehensive LaTeX user manual for the VGA Graphics Controller.

## Quick Start

### Prerequisites

You need a LaTeX distribution installed:

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get install texlive-full
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install texlive-scheme-full
```

**macOS:**
```bash
brew install --cask mactex
```

**Windows:**
Download and install MiKTeX or TeX Live from:
- MiKTeX: https://miktex.org/download
- TeX Live: https://www.tug.org/texlive/

### Building the PDF

Simply run:
```bash
make
```

This will create `vga_manual.pdf` in the current directory.

### Viewing the PDF

```bash
make view
```

### Cleaning Up

Remove auxiliary files:
```bash
make clean
```

Remove all generated files including the PDF:
```bash
make distclean
```

## Manual Contents

The manual includes:

1. **Introduction** - Overview and features
2. **Hardware Overview** - Architecture and block diagrams
3. **Getting Started** - Quick setup guide
4. **Memory Map Reference** - Complete address space documentation
5. **Register Descriptions** - Detailed register reference
6. **Programming Guide** - Complete C API with examples
7. **Display Modes** - 320×200 and 640×400 mode details
8. **Color Palette** - Palette programming and techniques
9. **Double-Buffering** - Animation techniques
10. **Hardware Integration** - MicroBlaze, RISC-V, Zynq integration
11. **Troubleshooting** - Common issues and solutions
12. **Technical Specifications** - Complete electrical and timing specs
13. **Examples and Tutorials** - Working code examples
14. **Appendix** - Reference tables and glossary

## Manual Statistics

- **Pages:** ~80-100 pages
- **Code Examples:** 20+ complete working examples
- **Tables:** 15+ reference tables
- **Figures:** Timing diagrams and architecture diagrams
- **Coverage:** Hardware, software, and integration

## File Size

The generated PDF is approximately 500-800 KB.

## Compilation Notes

The LaTeX source uses standard packages that are included in most LaTeX distributions:

- `geometry` - Page layout
- `hyperref` - PDF hyperlinks
- `listings` - Code syntax highlighting
- `xcolor` - Color support
- `fancyhdr` - Headers and footers
- `booktabs` - Professional tables
- `longtable` - Multi-page tables
- `amsmath` - Mathematical typesetting
- `bytefield` - Register diagrams

If you get errors about missing packages, install the full TeXLive distribution or install individual packages through your TeX distribution's package manager.

## Customization

The manual can be easily customized by editing `vga_manual.tex`:

- Change page size: Modify `\geometry` settings
- Change fonts: Add font packages in preamble
- Add/remove sections: Edit chapter structure
- Modify code style: Edit `\lstdefinestyle` definitions
- Change colors: Modify `hyperref` and `xcolor` settings

## Alternative Build Methods

### Using latexmk (Recommended for Automation)

```bash
latexmk -pdf vga_manual.tex
```

### Manual Build

```bash
pdflatex vga_manual.tex
pdflatex vga_manual.tex  # Run twice for TOC
pdflatex vga_manual.tex  # Run thrice for references
```

### Using Online LaTeX Editors

You can also compile this document using online services:
- Overleaf (https://www.overleaf.com)
- Papeeria (https://papeeria.com)

Just upload `vga_manual.tex` and compile.

## Troubleshooting

**"File not found" errors:**
- Make sure all packages are installed
- Try running `pdflatex` multiple times

**"Dimension too large" errors:**
- This shouldn't happen with this document
- If it does, check that images/tables aren't too large

**Slow compilation:**
- First compilation takes longer (5-10 seconds)
- Subsequent compilations are faster
- Use `pdflatex -interaction=batchmode` for automated builds

**Missing fonts:**
- Install the full TeXLive/MiKTeX distribution
- Or install specific font packages as needed

## Contributing

To contribute improvements to the manual:

1. Edit `vga_manual.tex`
2. Test compilation locally
3. Check that all cross-references work
4. Verify code examples are correct
5. Update revision history in appendix

## License

This documentation is provided as-is for use with the VGA Graphics Controller.
