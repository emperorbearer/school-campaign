from ninja import NinjaAPI, Schema
from typing import List, Optional
from .models import Poster, GlobalSettings

api = NinjaAPI(title="캠페인 포스터 API")

class PosterSchema(Schema):
    id: int
    title: str
    description: str
    image_url: str
    custom_duration: Optional[int] = None

@api.get("/posters", response=List[PosterSchema])
def list_posters(request):
    posters = Poster.objects.filter(is_active=True).order_by('order', '-id')
    result = []
    for poster in posters:
        result.append({
            "id": poster.id,
            "title": poster.title,
            "description": poster.description,
            "image_url": request.build_absolute_uri(poster.image.url) if poster.image else "",
            "custom_duration": poster.custom_duration,
        })
    return result

class ConfigSchema(Schema):
    slideshow_interval: int

@api.get("/config", response=ConfigSchema)
def get_config(request):
    config = GlobalSettings.objects.first()
    interval = config.slideshow_interval if config else 5
    return {"slideshow_interval": interval}
