import argparse
import numpy as np
import pandas as pd
import matplotlib.pyplot as plt

# math utils 

def sigmoid(x):
    out = np.empty_like(x)
    pos = x >= 0
    neg = ~pos
    out[pos] = 1.0 / (1.0 + np.exp(-x[pos]))
    z = np.exp(x[neg])
    out[neg] = z / (1.0 + z)
    return out

def softmax(logits):
    z = logits - logits.max(axis=1, keepdims=True)
    expz = np.exp(z)
    return expz / expz.sum(axis=1, keepdims=True)

def one_hot(y, K):
    oh = np.zeros((y.shape[0], K), dtype=np.float64)
    oh[np.arange(y.shape[0]), y] = 1.0
    return oh

def train_test_split(X, y, test_ratio=0.2, seed=42):
    rng = np.random.default_rng(seed)
    n = X.shape[0]
    idx = np.arange(n)
    rng.shuffle(idx)
    test_n = max(1, min(n - 1, int(round(n * test_ratio))))
    te = idx[:test_n]
    tr = idx[test_n:]
    return X[tr], y[tr], X[te], y[te]

def standardize_fit_transform(Xtr, Xte):
    mean = Xtr.mean(axis=0)
    std = Xtr.std(axis=0)
    std[std < 1e-12] = 1.0
    return (Xtr - mean) / std, (Xte - mean) / std

def confusion_matrix(y_true, y_pred, K):
    cm = np.zeros((K, K), dtype=int)
    for t, p in zip(y_true, y_pred):
        cm[t, p] += 1
    return cm

def metrics_from_cm(cm):
    # binary: rows=true [0,1], cols=pred [0,1]
    TN, FP = cm[0,0], cm[0,1]
    FN, TP = cm[1,0], cm[1,1]
    acc = (TP + TN) / max(1, (TP + TN + FP + FN))
    prec = TP / max(1, (TP + FP))
    rec  = TP / max(1, (TP + FN))
    f1   = 0.0 if (prec + rec) == 0 else 2 * prec * rec / (prec + rec)
    return acc, prec, rec, f1

def roc_curve_and_auc(y_true, score_pos):
    # y_true in {0,1}; score_pos in [0,1] or real-valued
    order = np.argsort(-score_pos)  # descending score
    y = y_true[order]
    s = score_pos[order]

    P = np.sum(y == 1)
    N = np.sum(y == 0)
    P = max(1, P)
    N = max(1, N)

    tpr = [0.0]
    fpr = [0.0]
    tp = 0
    fp = 0

    # step through thresholds
    for i in range(len(s)):
        if y[i] == 1:
            tp += 1
        else:
            fp += 1
        tpr.append(tp / P)
        fpr.append(fp / N)

    # AUC via trapezoidal rule
    auc = 0.0
    for i in range(1, len(tpr)):
        auc += (fpr[i] - fpr[i-1]) * (tpr[i] + tpr[i-1]) / 2.0
    return np.array(fpr), np.array(tpr), auc

# ------------------ model ------------------
class MLP:
    def __init__(self, D, H, K, seed=123):
        rng = np.random.default_rng(seed)
        s1 = np.sqrt(2.0 / (D + H))
        s2 = np.sqrt(2.0 / (H + K))
        self.W1 = rng.normal(0.0, s1, size=(D, H))
        self.b1 = np.zeros((H,), dtype=np.float64)
        self.W2 = rng.normal(0.0, s2, size=(H, K))
        self.b2 = np.zeros((K,), dtype=np.float64)

    def forward(self, X):
        z1 = X @ self.W1 + self.b1
        a1 = sigmoid(z1)
        z2 = a1 @ self.W2 + self.b2
        p = softmax(z2)
        return p, (X, a1, p)

    def loss(self, p, y):
        eps = 1e-12
        return -np.log(np.clip(p[np.arange(y.shape[0]), y], eps, 1.0)).mean()

    def backward(self, cache, y, l2=1e-4):
        X, a1, p = cache
        N = X.shape[0]
        K = p.shape[1]
        Y = one_hot(y, K)
        dz2 = (p - Y) / N

        dW2 = a1.T @ dz2 + l2 * self.W2
        db2 = dz2.sum(axis=0)

        da1 = dz2 @ self.W2.T
        dz1 = da1 * (a1 * (1.0 - a1))

        dW1 = X.T @ dz1 + l2 * self.W1
        db1 = dz1.sum(axis=0)
        return dW1, db1, dW2, db2

    def step(self, grads, lr=0.05):
        dW1, db1, dW2, db2 = grads
        self.W1 -= lr * dW1
        self.b1 -= lr * db1
        self.W2 -= lr * dW2
        self.b2 -= lr * db2

    def predict_proba(self, X):
        p, _ = self.forward(X)
        return p

    def predict(self, X):
        p = self.predict_proba(X)
        return np.argmax(p, axis=1)

# ------------------ plotting ------------------
def plot_curves(losses, acc_tr, acc_te, out_prefix):
    ep = np.arange(1, len(losses) + 1)

    plt.figure()
    plt.plot(ep, losses)
    plt.xlabel("Epoch")
    plt.ylabel("Cross-Entropy Loss")
    plt.title("Training Loss")
    plt.tight_layout()
    plt.savefig(out_prefix + "_loss.png", dpi=200)

    plt.figure()
    plt.plot(ep, acc_tr, label="train")
    plt.plot(ep, acc_te, label="test")
    plt.xlabel("Epoch")
    plt.ylabel("Accuracy")
    plt.title("Accuracy Curves")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_prefix + "_acc.png", dpi=200)

def plot_confusion_matrix(cm, class_names, out_path):
    plt.figure()
    plt.imshow(cm)
    plt.title("Confusion Matrix (Test)")
    plt.xlabel("Predicted")
    plt.ylabel("True")
    plt.xticks([0,1], class_names)
    plt.yticks([0,1], class_names)

    # annotate
    for i in range(cm.shape[0]):
        for j in range(cm.shape[1]):
            plt.text(j, i, str(cm[i, j]), ha="center", va="center")

    plt.tight_layout()
    plt.savefig(out_path, dpi=200)

def plot_roc(fpr, tpr, auc, out_path):
    plt.figure()
    plt.plot(fpr, tpr)
    plt.plot([0,1], [0,1])  # diagonal
    plt.xlabel("False Positive Rate")
    plt.ylabel("True Positive Rate")
    plt.title(f"ROC Curve (AUC={auc:.4f})")
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)

def plot_score_hist(score_pos, y_true, out_path):
    plt.figure()
    score0 = score_pos[y_true == 0]
    score1 = score_pos[y_true == 1]
    plt.hist(score0, bins=30, alpha=0.6, label="true=fail (0)")
    plt.hist(score1, bins=30, alpha=0.6, label="true=pass (1)")
    plt.xlabel("Predicted P(pass)")
    plt.ylabel("Count")
    plt.title("Predicted Probability Distribution (Test)")
    plt.legend()
    plt.tight_layout()
    plt.savefig(out_path, dpi=200)

# ------------------ main ------------------
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", default="python_learning_exam_performance.csv")
    ap.add_argument("--hidden", type=int, default=64)
    ap.add_argument("--epochs", type=int, default=300)
    ap.add_argument("--lr", type=float, default=0.05)
    ap.add_argument("--l2", type=float, default=1e-4)
    ap.add_argument("--test_ratio", type=float, default=0.2)
    ap.add_argument("--seed", type=int, default=42)
    ap.add_argument("--out", default="report_bp")
    args = ap.parse_args()

    df = pd.read_csv(args.csv)

    # label
    if "passed_exam" not in df.columns:
        raise SystemExit("Cannot find column: passed_exam")
    y = df["passed_exam"].astype(int).to_numpy()

    # avoid leakage & id
    drop_cols = ["passed_exam", "final_exam_score", "student_id"]
    for c in drop_cols:
        if c in df.columns:
            df = df.drop(columns=[c])

    # categorical -> one-hot
    Xdf = pd.get_dummies(df, dummy_na=True).fillna(0.0)
    X = Xdf.to_numpy(dtype=np.float64)

    Xtr, ytr, Xte, yte = train_test_split(X, y, test_ratio=args.test_ratio, seed=args.seed)
    Xtr, Xte = standardize_fit_transform(Xtr, Xte)

    D = Xtr.shape[1]
    K = 2
    class_names = ["fail", "pass"]

    model = MLP(D=D, H=args.hidden, K=K, seed=args.seed + 100)

    losses, acc_tr, acc_te = [], [], []
    rng = np.random.default_rng(args.seed + 999)

    for ep in range(1, args.epochs + 1):
        idx = rng.permutation(len(Xtr))
        Xb, yb = Xtr[idx], ytr[idx]  # full-batch, shuffled

        p, cache = model.forward(Xb)
        loss = model.loss(p, yb)
        grads = model.backward(cache, yb, l2=args.l2)
        model.step(grads, lr=args.lr)

        pred_tr = model.predict(Xtr)
        pred_te = model.predict(Xte)
        a_tr = (pred_tr == ytr).mean()
        a_te = (pred_te == yte).mean()

        losses.append(loss); acc_tr.append(a_tr); acc_te.append(a_te)

        if ep == 1 or ep % 10 == 0 or ep == args.epochs:
            print(f"Epoch {ep:4d} | loss={loss:.4f} | train_acc={a_tr:.4f} | test_acc={a_te:.4f}")

    # ----- report outputs -----
    plot_curves(losses, acc_tr, acc_te, args.out)

    prob_te = model.predict_proba(Xte)
    score_pos = prob_te[:, 1]          # P(pass)
    pred_te = np.argmax(prob_te, axis=1)

    cm = confusion_matrix(yte, pred_te, K)
    acc, prec, rec, f1 = metrics_from_cm(cm)
    fpr, tpr, auc = roc_curve_and_auc(yte, score_pos)

    plot_confusion_matrix(cm, class_names, args.out + "_cm.png")
    plot_roc(fpr, tpr, auc, args.out + "_roc.png")
    plot_score_hist(score_pos, yte, args.out + "_probhist.png")

    # save a metrics txt (easy to paste into report)
    with open(args.out + "_metrics.txt", "w", encoding="utf-8") as f:
        f.write("BP-MLP Binary Classification Report (passed_exam)\n")
        f.write(f"Features(after one-hot) D={D}, Hidden H={args.hidden}, Epochs={args.epochs}\n")
        f.write(f"Train N={len(Xtr)}, Test N={len(Xte)}\n\n")
        f.write("Test Metrics:\n")
        f.write(f"  Accuracy  = {acc:.4f}\n")
        f.write(f"  Precision = {prec:.4f}\n")
        f.write(f"  Recall    = {rec:.4f}\n")
        f.write(f"  F1        = {f1:.4f}\n")
        f.write(f"  AUC       = {auc:.4f}\n\n")
        f.write("Confusion Matrix (rows=true, cols=pred):\n")
        f.write(str(cm) + "\n")

    print("\n=== Saved report artifacts ===")
    print(args.out + "_loss.png")
    print(args.out + "_acc.png")
    print(args.out + "_cm.png")
    print(args.out + "_roc.png")
    print(args.out + "_probhist.png")
    print(args.out + "_metrics.txt")
    print("================================")

if __name__ == "__main__":
    main()
