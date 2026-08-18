# Breast Cancer Prediction — Notebook

This repository contains a self-contained Jupyter Notebook that trains and evaluates several machine learning models to predict breast cancer diagnosis (benign vs malignant) using a tabular dataset (`data.csv`). The notebook explores two feature-processing strategies, tunes models with cross-validated grid search, and builds a soft-voting ensemble from the top performers.

Files

- `cancer-prediction.ipynb`: Main Jupyter Notebook. Implements data loading, preprocessing (KNN imputation + feature binning variants), model pipelines and hyperparameter grids, cross-validated model tuning, ensemble building, evaluation (ROC, metrics), and a simple ablation analysis.
- `data.csv`: The input dataset expected by the notebook. Must contain a `diagnosis` column and either `id` or `patient_id`.

Quick overview

- Two feature sets are prepared from the raw features:
  - KNN-imputed numeric features (applies KNN imputation to selected lab-like columns)
  - Binned indicator features (discretizes selected lab-like columns into low/mid/high and adds missing indicators)
- A set of pipelines are defined and grid-searched (scoring: ROC AUC): Logistic Regression (L2), ElasticNet Logistic, Random Forest, LightGBM, MLP, SVM, KNN.
- Top performing models (by cross-validated AUROC) are assembled into a soft VotingClassifier, which is trained and evaluated on a hold-out test split.

Requirements

- Python 3.8+ (recommended)
- Libraries used (not exhaustive):
  - numpy, pandas, matplotlib
  - scikit-learn
  - lightgbm
  - joblib

A minimal pip install command to create an environment:

```powershell
python -m venv .venv; .\.venv\Scripts\Activate.ps1; pip install --upgrade pip; pip install numpy pandas scikit-learn matplotlib lightgbm joblib
```

How to run

1. Place your dataset at the repository root as `data.csv`. The notebook expects a `diagnosis` column with values where malignant cases are indicated by the letter `M` (case-insensitive). If the file has an `id` column it will be renamed to `patient_id` automatically; otherwise a `patient_id` column will be created.
2. Open `cancer-prediction.ipynb` with Jupyter or VS Code's Notebook editor.
3. Run cells from top-to-bottom. The notebook prints progress and will perform GridSearchCV over multiple models — this can be compute intensive depending on available CPU and grid sizes.

Notes and reproducibility

- The notebook sets a global `RANDOM_STATE = 42` for reproducibility.
- Grid search uses `StratifiedKFold` for cross-validation and is scored by ROC AUC.
- Some models (e.g., MLP, LightGBM) may use additional resources or take longer; reduce grid sizes or `cv_folds` if needed.

Outputs

- Cross-validated AUROC scores per model and feature set (printed in the notebook).
- A soft-voting ensemble trained on the top models and its AUROC on a hold-out test set (printed and visualized as an ROC curve).
- Optional: you can uncomment the joblib.dump lines in the notebook to save the final ensemble and scaler to disk.

Tips for improvement / Next steps

- Add a `requirements.txt` or `environment.yml` for exact dependency versions.
- Add unit tests and small sample data for quick CI checks.
- Add more careful feature engineering and missingness handling for clinical variables.
- Tune LightGBM with larger grids and use early stopping on a validation set.
- Persist model artifacts and add a simple inference script to load models and predict on new records.

License

- This project is provided for educational use. Add a license file if you intend to share or distribute it.

Contact

- If you want help extending this notebook (e.g., adding a REST inference API or converting to a reproducible pipeline), tell me what you'd like next.

## References

- Ostberg, N., & Peterson, D. (2020). Predicting a Decline in Patient Reported Outcomes for Cancer Patients on Chemotherapy. CS229 (Stanford University) — Final Project Write-up, Spring 2020.
