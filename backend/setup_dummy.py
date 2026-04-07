import os
import django
from django.core.files import File
from PIL import Image, ImageDraw, ImageFont
from io import BytesIO

# Setup Django Environment
os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'config.settings')
django.setup()

from api.models import Poster
from django.contrib.auth.models import User

# Add DB tables if not exist (makemigrations & migrate)
os.system('python manage.py makemigrations api')
os.system('python manage.py migrate')

# Create superuser if none
if not User.objects.filter(username='admin').exists():
    User.objects.create_superuser('admin', 'admin@example.com', 'admin')

# Generate Dummy Image Function
def create_dummy_poster(title, color, text):
    img = Image.new('RGB', (1920, 1080), color=color)
    d = ImageDraw.Draw(img)
    # Just simple text for dummy since we can't load complex fonts easily
    d.text((300, 500), text, fill=(255, 255, 255))
    
    thumb_io = BytesIO()
    img.save(thumb_io, format='JPEG')
    return File(thumb_io, name=f"{title}.jpg")

if not Poster.objects.exists():
    p1 = Poster(title="학교폭력은 범죄입니다", description="사소한 장난도 폭력이 될 수 있습니다.", order=1)
    p1.image.save('poster1.jpg', create_dummy_poster('poster1', '#ef4444', 'School Violence Prevention Campaign 1'))
    p1.save()

    p2 = Poster(title="배려와 존중이 함께하는 학교", description="문화와 책임규약을 지킵시다.", order=2)
    p2.image.save('poster2.jpg', create_dummy_poster('poster2', '#3b82f6', 'School Violence Prevention Campaign 2'))
    p2.save()

    p3 = Poster(title="방관자에서 방어자로!", description="용기 있는 행동이 친구를 구합니다.", order=3)
    p3.image.save('poster3.jpg', create_dummy_poster('poster3', '#10b981', 'School Violence Prevention Campaign 3'))
    p3.save()

print("Setup Complete")
