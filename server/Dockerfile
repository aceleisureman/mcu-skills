FROM python:3.12-slim

WORKDIR /app

COPY skills /app/skills
COPY tools /app/tools
COPY evals /app/evals
COPY docs /app/docs
COPY server /app/server
COPY README.md /app/README.md

WORKDIR /app/server
RUN pip install --no-cache-dir -r requirements.txt

ENV MCU_SKILLS_ROOT=/app \
    MCU_DATA_DIR=/data \
    PYTHONPATH=/app/server \
    MCU_HOST=0.0.0.0 \
    MCU_PORT=8080

VOLUME ["/data"]
EXPOSE 8080

CMD ["uvicorn", "app.main:app", "--host", "0.0.0.0", "--port", "8080"]
