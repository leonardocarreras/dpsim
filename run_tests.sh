#!/usr/bin/env bash
set -euo pipefail

source venv/bin/activate

# 1) Build dpsimpy + tests (linux-fedora-dpsimpy)
rm -rf build
mkdir -p build
cmake -S . -B build -DCIM_VERSION=CGMES_2.4.15_16FEB2016 -DCOVERAGE=ON
cmake --build build -j8

# 2) Notebook tests (test-jupyter-notebooks)
chmod -R +x ./build/dpsim/examples/cxx || true
git config --global --add safe.directory "$(pwd)"
ls -l ./build/dpsim/examples/cxx || true

python3 -m pip install -U pip
python3 -m pip install pytest pytest-xdist pytest-cov nbformat nbconvert

# Make python package importable like in CI
cp -r python/src/dpsim build/ 2>/dev/null || true
export PYTHONPATH="$(pwd)/build"

# Run pytest over all notebooks with coverage
pytest -n auto --cov --cov-branch --cov-report=xml examples/Notebooks/

# Archive notebook outputs (local: just ensure dir exists)
mkdir -p outputs

# 3) lcov capture & report (same commands as workflow)
# Install lcov (Fedora base like CI)
sudo dnf install -y lcov

# Capture coverage from current tree
lcov --capture --directory . --output-file coverage.info --rc lcov_branch_coverage=1 --ignore-errors mismatch
lcov --remove coverage.info '/usr/*' --output-file coverage.cleaned.info
lcov --list coverage.cleaned.info

# 4) Produce the same artifacts layout the workflow uploads
mkdir -p build/coverage_report
# (The workflow uploads multiple paths; we just ensure they exist locally.)
# Note: gcno/gcda files are created during build/run automatically.

echo
echo "Done."
echo "Artifacts you now have locally:"
echo "  - coverage.xml (pytest-cov)"
echo "  - coverage.info / coverage.cleaned.info (lcov)"
echo "  - build/** (compiled artifacts)"
echo "  - outputs/ (notebook outputs)"
