import os
import io
import csv
import json
import sqlite3
import time
import random
from collections import Counter
from configparser import ConfigParser
from flask import Flask, request, jsonify, render_template, Response
from groq import Groq

app = Flask(__name__)

def init_db():
    conn = sqlite3.connect('pie.db')
    c = conn.cursor()
    c.execute('''CREATE TABLE IF NOT EXISTS chat_session
                 (id INTEGER PRIMARY KEY AUTOINCREMENT, 
                  created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP,
                  summary TEXT,
                  profile_data TEXT,
                  chart_data TEXT)''')
    c.execute('''CREATE TABLE IF NOT EXISTS message
                 (id INTEGER PRIMARY KEY AUTOINCREMENT,
                  session_id INTEGER,
                  role TEXT,
                  content TEXT,
                  timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP)''')
    conn.commit()
    conn.close()

init_db()

# ──────────────────────────────────────────────
# ⚠️  PUT YOUR GROQ API KEY HERE
# ──────────────────────────────────────────────
GROQ_API_KEY = "YOUR_GROQ_API_KEY"
client = Groq(api_key=GROQ_API_KEY)


# ── helpers ──────────────────────────────────

def compute_summary(rows: list) -> str:
    """Build a human-readable session summary from the CSV data."""
    total_rows = len(rows)
    if total_rows == 0:
        return "Empty session"

    # Time range
    try:
        t_min = min(float(r["timestamp"]) for r in rows)
        t_max = max(float(r["timestamp"]) for r in rows)
        duration_s = t_max - t_min
    except (KeyError, ValueError):
        duration_s = 0

    # Behavior distribution
    behavior_counter = Counter(r.get("behavior", "") for r in rows)
    behavior_counts = {b: (c / total_rows) * 100 for b, c in behavior_counter.items() if b}

    # Amplitude stats
    amplitudes = [float(r["amplitude"]) for r in rows if r.get("amplitude")]
    if amplitudes:
        amp_mean = sum(amplitudes) / len(amplitudes)
        amp_max = max(amplitudes)
        amp_min = min(amplitudes)
    else:
        amp_mean = amp_max = amp_min = 0

    # Z-score stats
    zs = [float(r["z"]) for r in rows if r.get("z")]
    if zs:
        z_mean = sum(zs) / len(zs)
        z_max = max(zs)
    else:
        z_mean = z_max = 0

    # Transitions
    transitions = Counter(r.get("transition", "NONE") for r in rows if r.get("transition", "NONE") != "NONE")
    transition_str = ", ".join(
        f"{t}: {c}" for t, c in transitions.items()
    ) if len(transitions) > 0 else "No significant transitions"

    # Personal insight breakdown
    insight_counts = Counter(r.get("personalInsight", "") for r in rows if r.get("personalInsight"))
    insight_str = ", ".join(f"{k}: {v}" for k, v in insight_counts.items())

    # Context insight
    ctx_counts = Counter(r.get("contextInsight", "") for r in rows if r.get("contextInsight"))
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


def compute_user_profile(rows: list) -> dict:
    """Return normalised proportions of Active / Inactive / Irregular."""
    total_rows = len(rows)
    if total_rows == 0:
        return {"Active": 0.0, "Inactive": 0.0, "Irregular": 0.0}
    
    counter = Counter(r.get("behavior", "") for r in rows)
    counts = {b: c / total_rows for b, c in counter.items() if b}
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
        # Avoid pandas. read as string stream then convert to list of dicts.
        stream = io.StringIO(file.stream.read().decode("utf-8", errors="ignore"))
        reader = csv.DictReader(stream)
        rows = list(reader)
        
        # Normalize columns to support both original and new CSV formats
        for r in rows:
            if "smoothAmp" in r and "amplitude" not in r:
                r["amplitude"] = r["smoothAmp"]
            if "insight" in r and "personalInsight" not in r:
                r["personalInsight"] = r["insight"]
            if "context" in r and "contextInsight" not in r:
                r["contextInsight"] = r["context"]
                
    except Exception as e:
        print(f"Error parsing CSV: {e}")
        return jsonify({"error": f"Failed to read CSV: {str(e)}"}), 400

    if not rows:
        return jsonify({"error": "CSV file is empty"}), 400

    required_cols = {"timestamp", "amplitude", "z", "behavior", "transition",
                     "personalInsight", "contextInsight"}
                     
    actual_cols = set(rows[0].keys())
    if not required_cols.issubset(actual_cols):
        missing = required_cols - actual_cols
        print(f"CSV missing columns: {missing}")
        return jsonify({"error": f"CSV missing columns: {missing}"}), 400

    summary = compute_summary(rows)
    user_profile = compute_user_profile(rows)

    try:
        response = ask_groq(summary, user_profile)
    except Exception as e:
        return jsonify({"error": f"Groq API error: {str(e)}"}), 500

    downsample = max(1, len(rows) // 100)
    chart_data = []
    for i in range(0, len(rows), downsample):
        try:
            ts = float(rows[i].get("timestamp", 0))
            amp = float(rows[i].get("amplitude", 0))
            chart_data.append({"t": ts, "y": amp})
        except (ValueError, TypeError):
            pass

    # Extract forecasting data from the last row
    forecast = {
        "prediction": rows[-1].get("prediction", "UNKNOWN"),
        "action": rows[-1].get("action", "NONE")
    }

    conn = sqlite3.connect('pie.db')
    c = conn.cursor()
    c.execute("INSERT INTO chat_session (summary, profile_data, chart_data) VALUES (?, ?, ?)",
              (summary, json.dumps(user_profile), json.dumps(chart_data)))
    session_id = c.lastrowid
    
    c.execute("INSERT INTO message (session_id, role, content) VALUES (?, ?, ?)",
              (session_id, 'assistant', response))
    conn.commit()
    conn.close()

    return jsonify({
        "session_id": session_id,
        "response": response,
        "summary": summary,
        "profile": user_profile,
        "chartData": chart_data,
        "forecast": forecast
    })


@app.route("/history", methods=["GET"])
def get_history():
    try:
        conn = sqlite3.connect('pie.db')
        c = conn.cursor()
        c.execute("SELECT id, summary, profile_data, chart_data FROM chat_session ORDER BY id DESC LIMIT 1")
        session = c.fetchone()
        if not session:
            return jsonify({"history": []})
            
        s_id, summary, profile_data, chart_data = session
        
        c.execute("SELECT role, content, timestamp FROM message WHERE session_id = ? ORDER BY id ASC", (s_id,))
        messages = [{"role": r[0], "content": r[1], "timestamp": r[2]} for r in c.fetchall()]
        conn.close()
        
        return jsonify({
            "session_id": s_id,
            "summary": summary,
            "profile": json.loads(profile_data) if profile_data else {},
            "chartData": json.loads(chart_data) if chart_data else [],
            "history": messages
        })
    except Exception as e:
        print(f"Error fetching history: {e}")
        return jsonify({"error": "DB Error"}), 500

@app.route("/clear", methods=["POST"])
def clear_history():
    try:
        conn = sqlite3.connect('pie.db')
        c = conn.cursor()
        c.execute("DELETE FROM chat_session")
        c.execute("DELETE FROM message")
        conn.commit()
        conn.close()
        return jsonify({"status": "success"})
    except Exception as e:
        print(f"Error clearing history: {e}")
        return jsonify({"error": "DB Error"}), 500

@app.route("/chat", methods=["POST"])
def chat():
    data = request.get_json()
    if not data or "message" not in data:
        return jsonify({"error": "Missing message"}), 400

    user_message = data["message"]
    summary = data.get("summary", "")
    user_profile = data.get("profile", {})
    history = data.get("history", [])
    session_id = data.get("session_id")
    
    if session_id:
        conn = sqlite3.connect('pie.db')
        c = conn.cursor()
        c.execute("INSERT INTO message (session_id, role, content) VALUES (?, ?, ?)",
                  (session_id, 'user', user_message))
        conn.commit()
        conn.close()

    system_prompt = f"""You are a personalized AI assistant who understands the user's behavior like a close companion.

Session summary:
{summary}

User baseline behavior:
- Active: {user_profile.get('Active', 0):.2f}
- Inactive: {user_profile.get('Inactive', 0):.2f}
- Irregular: {user_profile.get('Irregular', 0):.2f}

Answer the user's questions about this data in a helpful, concise, and natural human-like manner. 
Do not use technical terms unless necessary. Keep responses brief. DO NOT include <think> or reasoning steps."""

    messages = [{"role": "system", "content": system_prompt}]
    for msg in history:
        if msg.get("role") in ["user", "assistant"]:
            messages.append({"role": msg["role"], "content": msg.get("content", "")})
            
    messages.append({"role": "user", "content": user_message})

    try:
        chat_completion = client.chat.completions.create(
            messages=messages,
            model="llama-3.3-70b-versatile",
            temperature=0.6,
            max_tokens=300,
        )
        response_text = chat_completion.choices[0].message.content
        
        if session_id:
            conn = sqlite3.connect('pie.db')
            c = conn.cursor()
            c.execute("INSERT INTO message (session_id, role, content) VALUES (?, ?, ?)",
                      (session_id, 'assistant', response_text))
            conn.commit()
            conn.close()
            
        return jsonify({"response": response_text})
    except Exception as e:
        print(f"Error in chat endpoint: {e}")
        return jsonify({"error": f"Groq API error: {str(e)}"}), 500


@app.route("/stream")
def stream():
    def generate():
        while True:
            # Simulate edge-device sensor polling
            sim_amp = round(random.uniform(45.0, 165.0), 1)
            yield f"data: {json.dumps({'amp': sim_amp})}\n\n"
            time.sleep(3)
            
    return Response(generate(), mimetype='text/event-stream')

if __name__ == "__main__":
    app.run(debug=True, port=5000, threaded=True)
