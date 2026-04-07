from django.db import models

class Poster(models.Model):
    title = models.CharField(max_length=200, help_text="포스터 제목")
    description = models.TextField(blank=True, help_text="포스터에 대한 설명이나 캠페인 문구")
    image = models.ImageField(upload_to='posters/', help_text="캠페인 포스터 이미지")
    is_active = models.BooleanField(default=True, help_text="현재 슬라이드쇼에 표시할지 여부")
    order = models.IntegerField(default=0, help_text="표시 순서 (낮을수록 먼저 표시)")
    custom_duration = models.IntegerField(null=True, blank=True, help_text="이 개별 포스터만의 표시 시간 (초 단위). 비워두면 기본 설정을 따릅니다.")

    class Meta:
        ordering = ['order', '-id']
        verbose_name = '포스터'
        verbose_name_plural = '포스터 목록'

    def __str__(self):
        return self.title

class GlobalSettings(models.Model):
    slideshow_interval = models.IntegerField(default=5, help_text="슬라이드 전환 간격 (초 단위)")

    class Meta:
        verbose_name = '설정'
        verbose_name_plural = '설정'

    def __str__(self):
        return "슬라이드쇼 설정"
