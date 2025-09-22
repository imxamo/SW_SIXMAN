# app.py
from flask import Flask
app = Flask(__name__)

@app.get("/")
def hello():
    return "Hello from school server!"

if __name__ == "__main__":
    app.run(host="116.124.191.174", port="15020")  # 외부 공개
