import sklearn
print(sklearn.__version__)


import pandas as pd
import numpy as np
from sklearn.model_selection import StratifiedKFold
from sklearn.naive_bayes import GaussianNB, MultinomialNB
from sklearn.metrics import accuracy_score, recall_score, classification_report, confusion_matrix, roc_auc_score, roc_curve, auc
from sklearn.base import BaseEstimator, ClassifierMixin
import matplotlib.pyplot as plt
import seaborn as sns
import warnings

warnings.filterwarnings("ignore")

# 設置中文字體顯示
plt.rcParams['font.sans-serif'] = ['SimHei', 'Arial Unicode MS', 'Microsoft YaHei', 'sans-serif']
plt.rcParams['axes.unicode_minus'] = False

# 自定義 Mixed Naive Bayes 支援 predict_proba
class MixedNB(BaseEstimator, ClassifierMixin):
    def __init__(self, continuous_idx, discrete_idx):
        self.continuous_idx = continuous_idx
        self.discrete_idx = discrete_idx
        self.gnb = GaussianNB()
        self.mnb = MultinomialNB()

    def fit(self, X, y):
        self.gnb.fit(X[:, self.continuous_idx], y)
        self.mnb.fit(np.abs(X[:, self.discrete_idx]), y)
        return self

    def predict(self, X, threshold=0.5):
        proba = self.predict_proba(X)[:, 1]
        return (proba >= threshold).astype(int)

    def predict_proba(self, X):
        log_prob_g = self.gnb.predict_log_proba(X[:, self.continuous_idx])
        log_prob_m = self.mnb.predict_log_proba(np.abs(X[:, self.discrete_idx]))
        log_prob = log_prob_g + log_prob_m
        prob = np.exp(log_prob)
        prob /= prob.sum(axis=1, keepdims=True)
        return prob

# 計算特異度(specificity)的函數
def calculate_specificity(y_true, y_pred):
    cm = confusion_matrix(y_true, y_pred)
    if cm.shape != (2, 2):
        raise ValueError("特異度計算需要二分類問題")
    tn, fp, fn, tp = cm.ravel()
    specificity = tn / (tn + fp)
    return specificity

# --- 資料處理 ---
df = pd.read_csv(r"D:\AI課程\Diabetes\Diabetes Health Dataset Analysis.csv")
df = df[df["Diagnosis"].notna()].copy()
df.drop(columns=["PatientID", "DoctorInCharge"], inplace=True)

X = df.drop(columns=["Diagnosis"])
y = df["Diagnosis"].astype(int)

continuous_cols = X.select_dtypes(include="float64").columns
discrete_cols = X.select_dtypes(include="int64").columns
X_cont = X[continuous_cols].values
X_disc = X[discrete_cols].values
X_mixed = np.hstack([X_cont, X_disc])
cont_idx = np.arange(X_cont.shape[1])
disc_idx = np.arange(X_cont.shape[1], X_mixed.shape[1])

# --- 特徵重要性視覺化函數 ---
def plot_gnb_importance(clf, feature_names):
    if hasattr(clf, "theta_") and hasattr(clf, "var_"):
        means_diff = np.abs(clf.theta_[1] - clf.theta_[0])
        var_sum = clf.var_[1] + clf.var_[0]
        importance = means_diff / np.sqrt(var_sum)
        sorted_idx = np.argsort(importance)
        plt.figure(figsize=(10, 6))
        plt.barh(range(len(sorted_idx)), importance[sorted_idx], align='center')
        plt.yticks(range(len(sorted_idx)), [feature_names[i] for i in sorted_idx])
        plt.xlabel("特徵重要性 (標準化均值差)")
        plt.title("連續特徵重要性（GaussianNB）")
        plt.tight_layout()
        plt.savefig('gaussian_feature_importance.png', dpi=300)
        plt.show()

def plot_mnb_importance(clf, feature_names):
    if hasattr(clf, "feature_log_prob_"):
        importance = clf.feature_log_prob_[1] - clf.feature_log_prob_[0]
        sorted_idx = np.argsort(importance)
        plt.figure(figsize=(10, 6))
        bars = plt.barh(range(len(sorted_idx)), importance[sorted_idx], align='center')
        plt.yticks(range(len(sorted_idx)), [feature_names[i] for i in sorted_idx])
        for bar in bars:
            width = bar.get_width()
            plt.text(width + (0.01 if width >= 0 else -0.05), 
                     bar.get_y() + bar.get_height()/2, 
                     f"{width:.2f}", 
                     va='center', 
                     ha='left' if width >= 0 else 'right',
                     fontsize=8)
        plt.xlabel("特徵重要性 (對數概率比)")
        plt.title("離散特徵重要性（MultinomialNB）")
        plt.tight_layout()
        plt.savefig('discrete_feature_importance.png', dpi=300)
        plt.show()

def plot_combined_feature_importance(mnb, gnb, cont_cols, disc_cols):
    mnb_series = pd.Series(mnb.feature_log_prob_[1] - mnb.feature_log_prob_[0], index=disc_cols)
    means_diff = np.abs(gnb.theta_[1] - gnb.theta_[0])
    var_sum = gnb.var_[1] + gnb.var_[0]
    gnb_series = pd.Series(means_diff / np.sqrt(var_sum), index=cont_cols)
    importance_df = pd.concat([mnb_series, gnb_series]).sort_values()
    plt.figure(figsize=(10, 8))
    colors = ['steelblue' if feat in disc_cols else 'darkorange' for feat in importance_df.index]
    bars = plt.barh(importance_df.index, importance_df.values, color=colors)
    for bar in bars:
        width = bar.get_width()
        plt.text(width + 0.01 * (1 if width >= 0 else -1), 
                 bar.get_y() + bar.get_height() / 2,
                 f"{width:.2f}", 
                 va='center', 
                 ha='left' if width >= 0 else 'right',
                 fontsize=9)
    from matplotlib.patches import Patch
    plt.legend(handles=[Patch(facecolor='steelblue', label='離散特徵'),
                        Patch(facecolor='darkorange', label='連續特徵')], loc='lower right')
    plt.title("混合 Naive Bayes 特徵重要性")
    plt.xlabel("特徵重要性值（離散: 對數概率比 | 連續: 標準化均值差）")
    plt.tight_layout()
    plt.savefig("combined_feature_importance.png", dpi=300)
    plt.show()

# --- 交叉驗證 ---
kf = StratifiedKFold(n_splits=8, shuffle=True, random_state=42)
best_model = None
best_recall = 0
best_metrics = {}
threshold = 0.3  # 調低門檻以提升 recall

for fold, (train_idx, test_idx) in enumerate(kf.split(X_mixed, y), 1):
    X_train, X_test = X_mixed[train_idx], X_mixed[test_idx]
    y_train, y_test = y.iloc[train_idx], y.iloc[test_idx]

    model = MixedNB(continuous_idx=cont_idx, discrete_idx=disc_idx)
    model.fit(X_train, y_train)
    y_pred = model.predict(X_test, threshold=threshold)
    y_proba = model.predict_proba(X_test)[:, 1]

    acc = accuracy_score(y_test, y_pred)
    rec = recall_score(y_test, y_pred)
    spec = calculate_specificity(y_test, y_pred)
    roc_auc = roc_auc_score(y_test, y_proba)
    
    print(f"Fold {fold}: Accuracy = {acc:.4f}, Recall = {rec:.4f}, Specificity = {spec:.4f}, ROC AUC = {roc_auc:.4f}")

    if rec > best_recall:
        best_recall = rec
        best_model = model
        best_metrics = {
            "y_true": y_test,
            "y_pred": y_pred,
            "y_proba": y_proba,
            "accuracy": acc,
            "recall": rec,
            "specificity": spec,
            "roc_auc": roc_auc,
            "report": classification_report(y_test, y_pred),
        }

# --- 特徵重要性視覺化 ---
cont_feature_names = continuous_cols.tolist()
disc_feature_names = discrete_cols.tolist()

print("\n📊 連續特徵（GaussianNB）重要性圖：")
plot_gnb_importance(best_model.gnb, cont_feature_names)

print("\n📊 離散特徵（MultinomialNB）重要性圖：")
plot_mnb_importance(best_model.mnb, disc_feature_names)

print("\n📊 混合特徵重要性圖：")
plot_combined_feature_importance(
    mnb=best_model.mnb,
    gnb=best_model.gnb,
    cont_cols=cont_feature_names,
    disc_cols=disc_feature_names
)

# --- 輸出最佳模型評估結果 ---
print("\n✅ Best Model Performance:")
print("-"*50)
print(f"Recall: {best_metrics['recall']:.4f}")
print("-"*50)
print(f"Specificity: {best_metrics['specificity']:.4f}")
print("-"*50)
print(f"Accuracy: {best_metrics['accuracy']:.4f}")
print("-"*50)
print(f"ROC AUC: {best_metrics['roc_auc']:.4f}")
print("-"*50)
print("Classification Report:\n")
print(best_metrics["report"])

# --- 混淆矩陣 ---
cm = confusion_matrix(best_metrics["y_true"], best_metrics["y_pred"])
plt.figure(figsize=(5, 4))
sns.heatmap(cm, annot=True, fmt='d', cmap="YlGnBu", 
            xticklabels=["No Diabetes", "Diabetes"], 
            yticklabels=["No Diabetes", "Diabetes"])
plt.xlabel("Predicted")
plt.ylabel("Actual")
plt.title(f"Confusion Matrix-Diabetes Health Dataset Analysis\n"
          f"Recall = {best_metrics['recall']:.2f}, "
          f"Specificity = {best_metrics['specificity']:.2f}")
plt.tight_layout()
plt.savefig('confusion_matrix.png', dpi=300)
plt.show()

# --- ROC 曲線 ---
fpr, tpr, thresholds = roc_curve(best_metrics["y_true"], best_metrics["y_proba"])
roc_auc_val = auc(fpr, tpr)

plt.figure(figsize=(6, 6))
plt.plot(fpr, tpr, color='darkorange', lw=2, label=f'ROC curve (AUC = {roc_auc_val:.4f})')
plt.plot([0, 1], [0, 1], color='navy', lw=1, linestyle='--')
plt.xlim([0.0, 1.0])
plt.ylim([0.0, 1.05])
plt.xlabel('False Positive Rate')
plt.ylabel('True Positive Rate')
plt.title('Receiver Operating Characteristic (ROC) Curve')
plt.legend(loc="lower right")
plt.grid(True)
plt.tight_layout()
plt.savefig('roc_curve.png', dpi=300)
plt.show()
