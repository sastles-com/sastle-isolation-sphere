from fastapi import FastAPI

app = FastAPI(title="Isolation Sphere Server")

@app.get("/")
async def root():
    return {"message": "Hello from Isolation Sphere Server"}

@app.get("/health")
async def health():
    return {"status": "ok"}
