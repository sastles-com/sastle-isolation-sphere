import google.generativeai as genai
from app.core.config import get_settings

settings = get_settings()

class GenerativeService:
    def __init__(self, model_name="gemini-pro"):
        genai.configure(api_key=settings.GEMINI_API_KEY)
        self.model = genai.GenerativeModel(model_name)

    def send_message(self, message: str):
        response = self.model.generate_content(message)
        return response.text
