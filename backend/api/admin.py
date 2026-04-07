from django.contrib import admin
from .models import Poster, GlobalSettings

@admin.register(Poster)
class PosterAdmin(admin.ModelAdmin):
    list_display = ('title', 'is_active', 'order', 'custom_duration')
    list_filter = ('is_active',)
    search_fields = ('title',)
    ordering = ('order', '-id')

@admin.register(GlobalSettings)
class GlobalSettingsAdmin(admin.ModelAdmin):
    list_display = ('__str__', 'slideshow_interval')

    def has_add_permission(self, request):
        if self.model.objects.count() >= 1:
            return False
        return super().has_add_permission(request)
