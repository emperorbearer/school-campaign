<script>
  import { onMount, onDestroy } from 'svelte';
  
  let posters = [];
  let currentIndex = 0;
  let timerId;
  let isLoading = true;
  let error = null;

  // The backend API URL. Make sure to adjust the host if needed.
  const API_URL = 'http://127.0.0.1:8000/api/posters';
  const CONFIG_URL = 'http://127.0.0.1:8000/api/config';
  let slideDuration = 5000;

  async function fetchPosters() {
    try {
      const response = await fetch(API_URL);
      if (!response.ok) {
        throw new Error('Failed to fetch posters');
      }
      const data = await response.json();
      posters = data;
      isLoading = false;
    } catch (e) {
      error = e.message;
      isLoading = false;
    }
  }

  function nextSlide() {
    if (posters.length > 1) {
      currentIndex = (currentIndex + 1) % posters.length;
      scheduleNextSlide();
    }
  }

  function scheduleNextSlide() {
    if (timerId) clearTimeout(timerId);
    if (posters.length > 1) {
      let currentPoster = posters[currentIndex];
      let currentDuration = (currentPoster.custom_duration ? currentPoster.custom_duration * 1000 : slideDuration);
      timerId = setTimeout(nextSlide, currentDuration);
    }
  }

  function startSlideshow() {
    scheduleNextSlide();
  }

  async function fetchConfig() {
    try {
      const response = await fetch(CONFIG_URL);
      if (response.ok) {
        const data = await response.json();
        if (data.slideshow_interval) {
          slideDuration = data.slideshow_interval * 1000;
        }
      }
    } catch (e) {
      console.warn("Config fetch failed, using default duration", e);
    }
  }

  onMount(async () => {
    await fetchConfig();
    await fetchPosters();
    startSlideshow();
  });

  onDestroy(() => {
    if (timerId) clearTimeout(timerId);
  });
</script>

<main>
  {#if isLoading}
    <div class="status-container">
      <div class="spinner"></div>
      <p>로딩 중...</p>
    </div>
  {:else if error}
    <div class="status-container error">
      <h1>⚠️ 에러 발생</h1>
      <p>{error}</p>
      <button on:click={() => window.location.reload()}>다시 시도하기</button>
    </div>
  {:else if posters.length === 0}
    <div class="status-container">
      <h1>학교폭력예방 캠페인</h1>
      <p>등록된 포스터가 없습니다.</p>
    </div>
  {:else}
    <div class="slideshow">
      {#each posters as poster, i}
        <div class="slide" class:active="{i === currentIndex}">
          <!-- Add dynamic image loading handling here if required -->
          <img src="{poster.image_url}" alt="{poster.title}" />
        </div>
      {/each}
      
      <!-- Progress Bar Tracker -->
      <div class="progress-container">
        <div class="progress-bar" style="width: {((currentIndex + 1) / posters.length) * 100}%"></div>
      </div>
    </div>
  {/if}
</main>
