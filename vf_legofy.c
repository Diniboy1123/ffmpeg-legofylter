/*
 * FFmpeg "legofy" filter - Applies a LEGO brick effect to videos.
 * Author: Diniboy1123 <github.com/Diniboy1123>
 * License: GPL
 * 
 * Idea from: https://github.com/JuanPotato/Legofy
 */

 #include "libavfilter/avfilter.h"
 #include "libavfilter/filters.h"
 #include "libavutil/opt.h"
 #include "libavutil/imgutils.h"
 #include "libavcodec/avcodec.h"
 #include "libavformat/avformat.h"
 #include "libavfilter/formats.h"
 #include "libavutil/time.h"
 
 typedef struct LegofyContext
 {
     const AVClass *class;
     int brick_size;
     char *brick_path;
     AVFrame *brick_texture;
 } LegofyContext;
 
 #define OFFSET(x) offsetof(LegofyContext, x)
 #define FLAGS AV_OPT_FLAG_FILTERING_PARAM | AV_OPT_FLAG_VIDEO_PARAM
 
 static const AVOption legofy_options[] = {
     {"brick_size", "Set size of LEGO brick", OFFSET(brick_size), AV_OPT_TYPE_INT, {.i64 = 16}, 2, 128, FLAGS},
     {"brick_path", "Path to LEGO brick PNG", OFFSET(brick_path), AV_OPT_TYPE_STRING, {.str = NULL}, 0, 0, FLAGS},
     {NULL}};
 
 AVFILTER_DEFINE_CLASS(legofy);
 
 static const enum AVPixelFormat pix_fmts[] = {
     AV_PIX_FMT_RGBA,
     AV_PIX_FMT_RGB24,
     AV_PIX_FMT_YUV420P,
     AV_PIX_FMT_NONE};
 
 /*-------------------------------
   Helper: Load and decode the brick PNG
 --------------------------------*/
 static int load_brick_texture(LegofyContext *legofy, AVFilterContext *ctx)
 {
     AVFormatContext *fmt_ctx = NULL;
     AVCodecContext *codec_ctx = NULL;
     AVCodec *codec = NULL;
     AVPacket packet;
     AVFrame *frame = av_frame_alloc();
     int ret;
 
     if (!frame)
         return AVERROR(ENOMEM);
 
     if ((ret = avformat_open_input(&fmt_ctx, legofy->brick_path, NULL, NULL)) < 0)
         goto fail;
 
     if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
         goto fail;
 
     codec = avcodec_find_decoder(fmt_ctx->streams[0]->codecpar->codec_id);
     if (!codec)
     {
         ret = AVERROR_DECODER_NOT_FOUND;
         goto fail;
     }
 
     codec_ctx = avcodec_alloc_context3(codec);
     if (!codec_ctx)
     {
         ret = AVERROR(ENOMEM);
         goto fail;
     }
     avcodec_parameters_to_context(codec_ctx, fmt_ctx->streams[0]->codecpar);
     if ((ret = avcodec_open2(codec_ctx, codec, NULL)) < 0)
         goto fail;
 
     while (av_read_frame(fmt_ctx, &packet) >= 0)
     {
         ret = avcodec_send_packet(codec_ctx, &packet);
         if (ret < 0)
             break;
 
         ret = avcodec_receive_frame(codec_ctx, frame);
         if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
         {
             av_packet_unref(&packet);
             continue;
         }
         if (ret < 0)
             break;
 
         legofy->brick_texture = av_frame_clone(frame);
         av_packet_unref(&packet);
         break;
     }
 
 fail:
     av_packet_unref(&packet);
     avcodec_free_context(&codec_ctx);
     if (fmt_ctx)
         avformat_close_input(&fmt_ctx);
     av_frame_free(&frame);
     return ret;
 }
 
 /*-----------------------------------------
   Core Processing: Apply the LEGO effect
 ------------------------------------------*/
 static int filter_frame(AVFilterLink *inlink, AVFrame *frame)
 {
     LegofyContext *legofy = inlink->dst->priv;
     int brick_size = legofy->brick_size;
     int width = frame->width;
     int height = frame->height;
     uint8_t *data = frame->data[0];
     int linesize = frame->linesize[0];
 
     if (!legofy->brick_texture)
     {
         av_log(inlink->dst, AV_LOG_ERROR, "Brick texture not loaded.\n");
         return AVERROR(EINVAL);
     }
 
     uint8_t *brick_data = legofy->brick_texture->data[0];
     int brick_linesize = legofy->brick_texture->linesize[0];
 
     for (int y = 0; y < height; y += brick_size)
     {
         for (int x = 0; x < width; x += brick_size)
         {
             int sum = 0, count = 0;
             // Compute average luma for the block
             for (int j = 0; j < brick_size && (y + j) < height; j++)
             {
                 for (int i = 0; i < brick_size && (x + i) < width; i++)
                 {
                     sum += data[(y + j) * linesize + (x + i)];
                     count++;
                 }
             }
             uint8_t avg = (uint8_t)(sum / count);
 
             // Resize and blend the brick texture onto this block
             for (int j = 0; j < brick_size && (y + j) < height; j++)
             {
                 for (int i = 0; i < brick_size && (x + i) < width; i++)
                 {
                     int bx = (i * legofy->brick_texture->width) / brick_size;
                     int by = (j * legofy->brick_texture->height) / brick_size;
 
                     bx = bx % legofy->brick_texture->width;
                     by = by % legofy->brick_texture->height;
 
                     uint8_t tex_pixel = brick_data[by * brick_linesize + bx];
 
                     data[(y + j) * linesize + (x + i)] = (avg * 0.6) + (tex_pixel * 0.4);
                 }
             }
         }
     }
     return ff_filter_frame(inlink->dst->outputs[0], frame);
 }
 
 /*----------------------------------------------------
   Activate Callback: Consume and process input frames
 
   Ugly, but I would need to study FFmpeg's internal's more
 -----------------------------------------------------*/
 static int legofy_activate(AVFilterContext *ctx) {
     AVFilterLink *inlink = ctx->inputs[0];
     AVFrame *frame;
     int ret;
 
     ret = ff_inlink_consume_frame(inlink, &frame);
     if (ret > 0) {
         return filter_frame(inlink, frame);
     } else if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
         return ret;
     }
 
     ff_inlink_request_frame(inlink);
     return 0;
 }
 
 
 /*--------------------------
   Initialization & Uninit
 ---------------------------*/
 static int init(AVFilterContext *ctx)
 {
     return 0;
 }
 
 static void uninit(AVFilterContext *ctx)
 {
     LegofyContext *legofy = ctx->priv;
     if (legofy->brick_texture)
         av_frame_free(&legofy->brick_texture);
 }
 
 /*-------------------------------------------
    Input Link Config: Load brick texture at start
  --------------------------------------------*/
 static int config_input(AVFilterLink *inlink)
 {
     LegofyContext *legofy = inlink->dst->priv;
     int ret = load_brick_texture(legofy, inlink->dst);
 
     if (ret < 0)
         return ret;
 
     ff_inlink_request_frame(inlink);
     return 0;
 }
 
 /*--------------------------------
   Input and Output Pad Definitions
 ---------------------------------*/
 static const AVFilterPad legofy_inputs[] = {
     {
         .name = "default",
         .type = AVMEDIA_TYPE_VIDEO,
         .config_props = config_input,
         .filter_frame = filter_frame,
     },
 };
 
 static const AVFilterPad legofy_outputs[] = {
     {
         .name = "default",
         .type = AVMEDIA_TYPE_VIDEO,
     },
 };
 
 const FFFilter ff_vf_legofy = {
     .p.name = "legofy",
     .p.description = "Applies a LEGO brick effect using an external LEGO PNG texture.",
     .p.priv_class = &legofy_class,
     .priv_size = sizeof(LegofyContext),
     .init = init,
     .uninit = uninit,
     .activate = legofy_activate,
     FILTER_INPUTS(legofy_inputs),
     FILTER_OUTPUTS(legofy_outputs),
     FILTER_PIXFMTS_ARRAY(pix_fmts),
 };
 