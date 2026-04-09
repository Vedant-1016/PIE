import os
import io
import pandas as pd
from flask import Flask, request, jsonify, render_template
from groq import Groq

app = Flask(__name__)

# ──────────────────────────────────────────────
# ⚠️  PUT YOUR GROQ API KEY HERE
# ──────────────────────────────────────────────
GROQ_API_KEY = "YOUR_GROQ_API_KEY_HERE"

client = Groq(api_key=GROQ_API_KEY)


# ── helpers ──────────────────────────────────

def compute_summary(df: pd.DataFrame) -> str:
    """Build a human-readable session summary from the CSV data."""
    total_rows = len(df)

    # Time range
    if "timestamp" in df.columns:
        t_min, t_max = df["timestamp"].min(), df["timestamp"].max()
        duration_s = t_max - t_min
    else:
        duration_s = 0

    # Behavior distribution
    behavior_counts = df["behavior"].value_counts(normalize=True) * 100

    # Amplitude stats
    amp_mean = df["amplitude"].mean()
    amp_max = df["amplitude"].max()
    amp_min = df["amplitude"].min()

    # Z-score stats
    z_mean = df["z"].mean()
    z_max = df["z"].max()

    # Transitions
    transitions = df[df["transition"] != "NONE"]["transition"].value_counts()
    transition_str = ", ".join(
        f"{t}: {c}" for t, c in transitions.items()
    ) if len(transitions) > 0 else "No significant transitions"

    # Personal insight breakdown
    insight_counts = df["personalInsight"].value_counts()
    insight_str = ", ".join(f"{k}: {v}" for k, v in insight_counts.items())

    # Context insight
    ctx_counts = df["contextInsight"].value_counts()
    ctx_str = ", ".join(f"{k}: {v}" for k, v in ctx_counts.items())

    summary = (
        f"Session contains {total_rows} data points over {duration_s:.1f} seconds.\n"
        f"Behavior breakdown: {', '.join(f'{b}: {p:.1f}%' for b, p in behavior_counts.items())}.\n"
        f"Amplitude — mean: {amp_mean:.1f}, min: {amp_min:.1f}, max: {amp_max:.1f}.\n"
        f"Z-score — mean: {z_mean:.2f}, max: {z_max:.2f}.\n"
        f"Transitions detected: {transition_str}.\n"
        f"Personal insights: {insight_str}.\n"
        f"Context insights: {ctx_str}."
    )
    return summary


def compute_user_profile(df: pd.DataFrame) -> dict:
    """Return normalised proportions of Active / Inactive / Irregular."""
    counts = df["behavior"].value_counts(normalize=True)
    return {
        "Active": counts.get("ACTIVE", 0.0),
        "Inactive": counts.get("INACTIVE", 0.0),
        "Irregular": counts.get("MIXED", 0.0) + counts.get("IRREGULAR", 0.0),
    }


def ask_groq(summary: str, user_profile: dict) -> str:
    """Send the personalized prompt to Groq and return the response."""
    prompt = f"""
You are a personalized AI assistant who understands the user's behavior like a close companion.

Session summary:
{summary}

User baseline behavior:
- Active: {user_profile['Active']:.2f}
- Inactive: {user_profile['Inactive']:.2f}
- Irregular: {user_profile['Irregular']:.2f}

Your task:
1. What happened
2. Why it happened (based on user pattern)
3. What the user should do

Rules:
- Keep it short (3-4 lines max)
- Be natural and human-like
- Avoid technical terms
- DO NOT include <think> or reasoning steps
- Be concise and do not repeat information

Format:
Observation:
Reason:
Suggestion:
"""

    chat_completion = client.chat.completions.create(
        messages=[{"role": "user", "content": prompt}],
        model="llama-3.3-70b-versatile",
        temperature=0.6,
        max_tokens=300,
    )
    return chat_completion.choices[0].message.content


# ── routes ───────────────────────────────────

@app.route("/")
def index():
    return render_template("index.html")


@app.route("/analyze", methods=["POST"])
def analyze():
    if "file" not in request.files:
        return jsonify({"error": "No file uploaded"}), 400

    file = request.files["file"]
    if file.filename == "":
        return jsonify({"error": "Empty filename"}), 400

    try:
        stream = io.StringIO(file.stream.read().decode("utf-8"))
        df = pd.read_csv(stream)
    except Exception as e:
        return jsonify({"error": f"Failed to parse CSV: {str(e)}"}), 400

    required_cols = {"timestamp", "amplitude", "z", "behavior", "transition",
                     "personalInsight", "contextInsight"}
    if not required_cols.issubset(set(df.columns)):
        missing = required_cols - set(df.columns)
        return jsonify({"error": f"CSV missing columns: {missing}"}), 400

    summary = compute_summary(df)
    user_profile = compute_user_profile(df)

    try:
        response = ask_groq(summary, user_profile)
    except Exception as e:
        return jsonify({"error": f"Groq API error: {str(e)}"}), 500

    return jsonify({
        "response": response,
        "summary": summary,
        "profile": user_profile,
    })


if __name__ == "__main__":
    app.run(debug=True, port=5000)
