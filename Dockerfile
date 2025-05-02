FROM python:3.11-slim

RUN apt update && apt install -y libgl1-mesa-glx libxrender1 libsm6 libxext6 build-essential \
    && pip install --no-cache-dir fastapi uvicorn pythonocc-core

COPY . /app
WORKDIR /app

CMD ["uvicorn", "main:app", "--host", "0.0.0.0", "--port", "8000"]
