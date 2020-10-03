# foo_mpv
mpv video player UI element for foobar2000.

<img src="screenshot.png" width="660">

### Features
- For playback of local video files
- A/V sync should be approximately as fast and accurate as a standalone video player most of the time
- Optionally acts as an album art display when no video is available
- On-screen control based on mpv's, can be enabled/disabled per-UI element
- Thumbnail generator for providing album art for videos to foobar, with an on-disk cache
- Thumbnails are used as a fallback to other art sources, which allows mixing thumbnails and box art in different views, eg:
<img src="screenshot2.png" width="380">

- Thumbnail picker for visually choosing a thumbnail for a video
- Algorithm for automatically choosing 'good' thumbnails
- DefaultUI element, Columns UI panel, standalone popup and fullscreen mode
- Single video instance moves to the largest currently visible UI element unless manually pinned to one area via the context menu, making it simple to switch between a small instance and larger instance in your layout
- Developed to work well in Wine
- Can read mpv.conf from `mpv/mpv.conf` in your foobar2000 profile folder

NB: be careful choosing options in mpv.conf. You probably don't want to override any of the options set by the component or performance might suffer. Good options to set might be scaling, video filters, deinterlacing, etc. You can specify different profiles for use when displaying video and album art using `[video]` and `[albumart]`. For example, a good place to start might be:

```
profile=gpu-hq

[video]
vf=bwdif:deint=1 # deinterlace videos automatically

[albumart]
vf=              # turn off video filters for album art so that PNG transparency works
```

The mpv profile folder is set to `<foobar profile>/mpv`, so you can use paths relative to this in `mpv.conf` by using `~~` as normal.
