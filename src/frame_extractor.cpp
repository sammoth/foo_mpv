#pragma once
#include "stdafx.h"
// PCH ^

#include "../SDK/foobar2000.h"
#include "frame_extractor.h"

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/frame.h"
#include "libswscale/swscale.h"
}

namespace mpv {
static void libavtry(int error) {
  if (error < 0) {
    char* error_str = new char[500];
    av_strerror(error, error_str, 500);
    console::error(error_str);
    delete[] error_str;

    throw exception_album_art_unsupported_entry();
  }
}

frame_extractor::frame_extractor(art_image_format format, const char* path)
    : found(false) {
  filename.add_filename(path);
  if (filename.has_prefix("\\file://")) {
    filename.remove_chars(0, 8);

    if (filename.is_empty()) return;
  } else {
    filename.reset();
    return;
  }

  p_format_context = avformat_alloc_context();
  libavtry(avformat_open_input(&p_format_context, filename.c_str(), NULL, NULL));

  libavtry(avformat_find_stream_info(p_format_context, NULL));

  stream_index = -1;

  for (unsigned i = 0; i < p_format_context->nb_streams; i++) {
    AVCodecParameters* local_codec_params =
        p_format_context->streams[i]->codecpar;

    if (local_codec_params->codec_type == AVMEDIA_TYPE_VIDEO) {
      codec = avcodec_find_decoder(local_codec_params->codec_id);
      if (codec == NULL) continue;

      stream_index = i;
      params = local_codec_params;
      break;
    }
  }

  if (stream_index == -1) throw exception_album_art_not_found();

  p_codec_context = avcodec_alloc_context3(codec);
  libavtry(avcodec_parameters_to_context(p_codec_context, params));
  libavtry(avcodec_open2(p_codec_context, codec, NULL));

  switch (format) {
    case jpeg:
      output_pixelformat = AV_PIX_FMT_YUVJ444P;
      output_encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
      break;
    case bitmap:
      output_pixelformat = AV_PIX_FMT_BGR24;
      output_encoder = avcodec_find_encoder(AV_CODEC_ID_BMP);
      break;
  }

  output_codeccontext = avcodec_alloc_context3(output_encoder);
  output_packet = av_packet_alloc();
  outputformat_frame = av_frame_alloc();

  p_packet = av_packet_alloc();
  p_frame = av_frame_alloc();
}

frame_extractor::~frame_extractor() {
  av_frame_free(&outputformat_frame);
  av_packet_free(&output_packet);
  avcodec_free_context(&output_codeccontext);

  av_packet_free(&p_packet);
  av_frame_free(&p_frame);
  avcodec_free_context(&p_codec_context);
  avformat_free_context(p_format_context);
}

void frame_extractor::seek_percent(float percent) {
  libavtry(av_seek_frame(p_format_context, -1,
                      (int64_t)(0.01 * percent * p_format_context->duration), 0));
}

album_art_data_ptr frame_extractor::get_art() {
  int i = 0;

  while (av_read_frame(p_format_context, p_packet) >= 0 && i < 100) {
    if (p_packet->stream_index != stream_index) continue;

    i++;

    if (avcodec_send_packet(p_codec_context, p_packet) < 0) continue;

    if (avcodec_receive_frame(p_codec_context, p_frame) < 0) continue;

    break;
  }

  if (p_frame == NULL) return album_art_data_ptr();

  if (outputformat_frame == NULL) return album_art_data_ptr();

  outputformat_frame->format = output_pixelformat;
  outputformat_frame->width = p_frame->width;
  outputformat_frame->height = p_frame->height;
  libavtry(av_frame_get_buffer(outputformat_frame, 32));

  SwsContext* swscontext = sws_getContext(
      p_frame->width, p_frame->height, (AVPixelFormat)p_frame->format,
      p_frame->width, p_frame->height, output_pixelformat, 0, 0, 0, 0);

  if (swscontext == NULL) return album_art_data_ptr();

  sws_scale(swscontext, p_frame->data, p_frame->linesize, 0, p_frame->height,
            outputformat_frame->data, outputformat_frame->linesize);

  output_codeccontext->width = p_frame->width;
  output_codeccontext->height = p_frame->height;
  output_codeccontext->pix_fmt = output_pixelformat;
  output_codeccontext->time_base = AVRational{1, 1};

  libavtry(avcodec_open2(output_codeccontext, output_encoder, NULL));

  libavtry(avcodec_send_frame(output_codeccontext, outputformat_frame));

  if (output_packet == NULL) return album_art_data_ptr();

  libavtry(avcodec_receive_packet(output_codeccontext, output_packet));

  auto pic =
      album_art_data_impl::g_create(output_packet->data, output_packet->size);

  return pic;
}
}  // namespace mpv
