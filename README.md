# foo_mpv
mpv video player UI element for foobar2000.

<img src="screenshot.png" width="660">

### Features
- Local files only
- Aims to quickly, accurately and continuously sync video to the audio
- Optionally acts as an album art display when no video is available
- Thumbnail generator for providing album art for videos, with a cache
- Allows mixing thumbnails and box art in different views. So you can achieve something like this:
<img src="screenshot2.png" width="380">

- Ability to choose a thumbnail manually from a video
- Algorithm for automatically choosing 'good' thumbnails
- DefaultUI element, Columns UI panel, standalone popup and fullscreen mode
- Single video instance moves to the largest currently visible UI element unless manually pinned to one area via the context menu, making it simple to switch between a small instance and larger instance in your layout
- Developed to work in Wine
- Can read mpv.conf from `mpv/mpv.conf` in your foobar2000 profile folder

NB: go easy on the options in mpv.conf, you probably don't want to override any of the options set by the component or weird things might happen. Good options to set might be scaling, video filters, deinterlacing, etc. For example:

```
vf-append=bwdif:deint=1
profile=gpu-hq
```

The mpv profile folder is set to `<foobar profile>/mpv`, so you can use paths relative to this in `mpv.conf` by using `~~` as normal.
