/*
 * Copyright (C) 2006 Claudio Ciccani <klan@users.sf.net>
 *
 * Graphics driver for ATI R100 based chipsets written by
 *             Claudio Ciccani <klan@users.sf.net>.  
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>

#include <directfb.h>

#include <direct/messages.h>

#include <core/coredefs.h>
#include <core/coretypes.h>

#include <core/state.h>
#include <core/gfxcard.h>
#include <core/surfaces.h>

#include <gfx/convert.h>

#include "r100.h"
#include "r100_regs.h"
#include "r100_mmio.h"

#include "r100_state.h"


#define R100_IS_SET( flag ) \
     ((rdev->set & SMF_##flag) == SMF_##flag)

#define R100_SET( flag ) \
     rdev->set |= SMF_##flag

#define R100_UNSET( flag ) \
     rdev->set &= ~(SMF_##flag)


static __u32 r100SrcBlend[] = {
     SRC_BLEND_GL_ZERO,                 // DSBF_ZERO
     SRC_BLEND_GL_ONE,                  // DSBF_ONE
     SRC_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     SRC_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     SRC_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     SRC_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     SRC_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     SRC_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     SRC_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     SRC_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     SRC_BLEND_GL_SRC_ALPHA_SATURATE    // DSBF_SRCALPHASAT
};

static __u32 r100DstBlend[] = {
     DST_BLEND_GL_ZERO,                 // DSBF_ZERO
     DST_BLEND_GL_ONE,                  // DSBF_ONE
     DST_BLEND_GL_SRC_COLOR,            // DSBF_SRCCOLOR
     DST_BLEND_GL_ONE_MINUS_SRC_COLOR,  // DSBF_INVSRCCOLOR
     DST_BLEND_GL_SRC_ALPHA,            // DSBF_SRCALPHA
     DST_BLEND_GL_ONE_MINUS_SRC_ALPHA,  // DSBF_INVSRCALPHA
     DST_BLEND_GL_DST_ALPHA,            // DSBF_DSTALPHA
     DST_BLEND_GL_ONE_MINUS_DST_ALPHA,  // DSBF_INVDSTALPHA
     DST_BLEND_GL_DST_COLOR,            // DSBF_DSTCOLOR
     DST_BLEND_GL_ONE_MINUS_DST_COLOR,  // DSBF_INVDSTCOLOR
     DST_BLEND_GL_ZERO                  // DSBF_SRCALPHASAT
};



void r100_set_destination( R100DriverData *rdrv,
                           R100DeviceData *rdev,
                           CardState      *state )
{
     CoreSurface   *surface = state->destination;
     SurfaceBuffer *buffer  = surface->back_buffer;
     volatile __u8 *mmio    = rdrv->mmio_base;
     __u32          offset;
     __u32          pitch;
    
     if (R100_IS_SET( DESTINATION ))
          return;

     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 64) == 0 );

     offset = rdev->fb_offset + buffer->video.offset;
     pitch  = buffer->video.pitch;
    
     if (rdev->dst_offset != offset        ||
         rdev->dst_pitch  != pitch         ||
         rdev->dst_format != buffer->format)
     {
          bool dst_422 = false;
          
          switch (buffer->format) {
               case DSPF_LUT8:
               case DSPF_ALUT44:
               case DSPF_A8:
                    rdev->dp_gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    break;
               case DSPF_RGB332:          
                    rdev->dp_gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB332 | DITHER_ENABLE;
                    break;
               case DSPF_ARGB2554:
                    rdev->dp_gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB565;
                    break;
               case DSPF_ARGB4444:
                    rdev->dp_gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB4444 | DITHER_ENABLE;
                    break;
               case DSPF_ARGB1555:          
                    rdev->dp_gui_master_cntl = GMC_DST_15BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB1555 | DITHER_ENABLE;
                    break;
               case DSPF_RGB16:
                    rdev->dp_gui_master_cntl = GMC_DST_16BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB565 | DITHER_ENABLE;
                    break;
               case DSPF_RGB32:
               case DSPF_ARGB:
               case DSPF_AiRGB:
                    rdev->dp_gui_master_cntl = GMC_DST_32BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_ARGB8888;
                    break;
               case DSPF_UYVY:
                    rdev->dp_gui_master_cntl = GMC_DST_YVYU;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_YVYU;
                    dst_422 = true;
                    break;
               case DSPF_YUY2:
                    rdev->dp_gui_master_cntl = GMC_DST_VYUY;
                    rdev->rb3d_cntl = COLOR_FORMAT_YUV422_VYUY;
                    dst_422 = true;
                    break;
               case DSPF_I420:
                    rdev->dp_gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    rdev->dst_offset_cb = offset + pitch * surface->height;
                    rdev->dst_offset_cr = rdev->dst_offset_cb + 
                                          pitch * surface->height / 4;
                    break;
               case DSPF_YV12:
                    rdev->dp_gui_master_cntl = GMC_DST_8BPP;
                    rdev->rb3d_cntl = COLOR_FORMAT_RGB8;
                    rdev->dst_offset_cr = offset + pitch * surface->height;
                    rdev->dst_offset_cb = rdev->dst_offset_cr +
                                          pitch * surface->height / 4;
                    break;
               default:
                    D_BUG( "unexpected pixelformat" );
                    break;
          }

          rdev->dp_gui_master_cntl |= GMC_WR_MSK_DIS            |
                                      GMC_SRC_PITCH_OFFSET_CNTL |
                                      GMC_DST_PITCH_OFFSET_CNTL |
                                      GMC_DST_CLIPPING;
          
          r100_waitfifo( rdrv, rdev, 2 ); 
          r100_out32( mmio, DST_OFFSET, offset );
          r100_out32( mmio, DST_PITCH,  pitch );
          
          r100_waitfifo( rdrv, rdev, 2 );
          r100_out32( mmio, RB3D_COLOROFFSET, offset );
          r100_out32( mmio, RB3D_COLORPITCH,  pitch /
                                              DFB_BYTES_PER_PIXEL(buffer->format) );
          
          if (surface->caps & DSCAPS_DEPTH) {
               SurfaceBuffer *depth = surface->depth_buffer;
               
               offset = rdev->fb_offset + depth->video.offset;
               pitch  = depth->video.pitch >> 1;
               
               r100_waitfifo( rdrv, rdev, 3 );
               r100_out32( mmio, RB3D_DEPTHOFFSET, offset );
               r100_out32( mmio, RB3D_DEPTHPITCH,  pitch );
               r100_out32( mmio, RB3D_ZSTENCILCNTL, DEPTH_FORMAT_16BIT_INT_Z |
                                                    Z_TEST_ALWAYS );
          
               rdev->rb3d_cntl |= Z_ENABLE;
          }
          
          if (rdev->dst_format != buffer->format) {
               if (dst_422 && !rdev->dst_422) {
                    R100_UNSET( SOURCE );
                    R100_UNSET( CLIP );
               }
               
               R100_UNSET( COLOR );
               R100_UNSET( SRC_BLEND );
          }
          
          rdev->dst_format = buffer->format;
          rdev->dst_offset = offset;
          rdev->dst_pitch  = pitch;
          rdev->dst_422    = dst_422;
     }

     R100_SET( DESTINATION );
}

void r100_set_source( R100DriverData *rdrv,
                      R100DeviceData *rdev,
                      CardState      *state )
{
     CoreSurface   *surface  = state->source;
     SurfaceBuffer *buffer   = surface->front_buffer;
     volatile __u8 *mmio     = rdrv->mmio_base;
     __u32          txformat = TXFORMAT_NON_POWER2;
     __u32          txfilter = MAG_FILTER_LINEAR  |
                               MIN_FILTER_LINEAR  |
                               CLAMP_S_CLAMP_LAST |
                               CLAMP_T_CLAMP_LAST;

     if (R100_IS_SET( SOURCE )) {
          if ((state->blittingflags & DSBLIT_DEINTERLACE) ==
              (rdev->blittingflags  & DSBLIT_DEINTERLACE))
               return;
     }

     D_ASSERT( (buffer->video.offset % 32) == 0 );
     D_ASSERT( (buffer->video.pitch % 64) == 0 );

     rdev->src_offset = rdev->fb_offset + buffer->video.offset;
     rdev->src_pitch  = buffer->video.pitch;
     rdev->src_width  = surface->width  - 1;
     rdev->src_height = surface->height - 1;
     
     switch (buffer->format) {
          case DSPF_LUT8:
               txformat |= TXFORMAT_I8;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ALUT44:
               txformat |= TXFORMAT_I8;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x0000000f;
               break;
          case DSPF_A8:
               txformat |= TXFORMAT_I8 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0;
               break;
          case DSPF_RGB332:
               txformat |= TXFORMAT_RGB332;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_ARGB2554:
               txformat |= TXFORMAT_RGB565;
               txfilter &= ~(MAG_FILTER_LINEAR |
                             MIN_FILTER_LINEAR);
               rdev->src_mask = 0x00003fff;
               break;
          case DSPF_ARGB4444:
               txformat |= TXFORMAT_ARGB4444 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00000fff;
               break;
          case DSPF_ARGB1555:
               txformat |= TXFORMAT_ARGB1555 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00007fff;
               break;
          case DSPF_RGB16:
               txformat |= TXFORMAT_RGB565;
               rdev->src_mask = 0x0000ffff;
               break;
          case DSPF_RGB32:
               txformat |= TXFORMAT_ARGB8888;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_ARGB:
          case DSPF_AiRGB:
               txformat |= TXFORMAT_ARGB8888 |
                           TXFORMAT_ALPHA_IN_MAP;
               rdev->src_mask = 0x00ffffff;
               break;
          case DSPF_UYVY:
               txformat |= TXFORMAT_YVYU422;
               if (!rdev->dst_422)
                    txfilter |= YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_YUY2:
               txformat |= TXFORMAT_VYUY422;
               if (!rdev->dst_422)
                    txfilter |= YUV_TO_RGB;
               rdev->src_mask = 0xffffffff;
               break;
          case DSPF_I420:
               txformat |= TXFORMAT_I8;
               rdev->src_offset_cb = rdev->src_offset +
                                     rdev->src_pitch * surface->height;
               rdev->src_offset_cr = rdev->src_offset_cb +
                                     rdev->src_pitch * surface->height/4;
               rdev->src_mask = 0x000000ff;
               break;
          case DSPF_YV12:
               txformat |= TXFORMAT_I8;
               rdev->src_offset_cr = rdev->src_offset +
                                     rdev->src_pitch * surface->height;
               rdev->src_offset_cb = rdev->src_offset_cr +
                                     rdev->src_pitch * surface->height/4;
               rdev->src_mask = 0x000000ff;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               break;
     }

     if (state->blittingflags & DSBLIT_DEINTERLACE) { 
          rdev->src_height /= 2;
          if (surface->caps & DSCAPS_SEPARATED) {
               if (surface->field) {
                    rdev->src_offset    += rdev->src_height * rdev->src_pitch;
                    rdev->src_offset_cr += rdev->src_height * rdev->src_pitch/4;
                    rdev->src_offset_cb += rdev->src_height * rdev->src_pitch/4;
               }
          } else {
               if (surface->field) {
                    rdev->src_offset    += rdev->src_pitch;
                    rdev->src_offset_cr += rdev->src_pitch/2;
                    rdev->src_offset_cb += rdev->src_pitch/2;
               }
               rdev->src_pitch *= 2;
          }
     }

#ifdef WORDS_BIGENDIAN
     if (rdev->src_format != buffer->format) {
          switch (DFB_BYTES_PER_PIXEL(buffer->format)) {
               case 2:
                    r100_out32( mmio, SURFACE_CNTL, (rdev->surface_cntl 
                                                     & ~NONSURF_AP0_SWP_32BPP)
                                                    |  NONSURF_AP0_SWP_16BPP );
                    break;
               case 4:
                    r100_out32( mmio, SURFACE_CNTL, (rdev->surface_cntl
                                                     & ~NONSURF_AP0_SWP_16BPP)
                                                    |  NONSURF_AP0_SWP_32BPP );
                    break;
               default:
                    r100_out32( mmio, SURFACE_CNTL, rdev->surface_cntl
                                                    & ~(NONSURF_AP0_SWP_16BPP |
                                                        NONSURF_AP0_SWP_32BPP) );
                    break;
          }
     }
#endif
     
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, SRC_OFFSET, rdev->src_offset );
     r100_out32( mmio, SRC_PITCH,  rdev->src_pitch );
           
     r100_waitfifo( rdrv, rdev, 5 );
     r100_out32( mmio, PP_TXFILTER_0, txfilter );
     r100_out32( mmio, PP_TXFORMAT_0, txformat );
     r100_out32( mmio, PP_TEX_SIZE_0, (rdev->src_height << 16) |
                                      (rdev->src_width & 0xffff) );
     r100_out32( mmio, PP_TEX_PITCH_0, rdev->src_pitch - 32 );
     r100_out32( mmio, PP_TXOFFSET_0, rdev->src_offset );
     
     if (rdev->src_format != buffer->format)
          R100_UNSET( BLITTING_FLAGS );
     rdev->src_format = buffer->format;

     R100_SET( SOURCE );
}

void r100_set_clip( R100DriverData *rdrv,
                    R100DeviceData *rdev,
                    CardState      *state )
{
     DFBRegion     *clip = &state->clip;
     volatile __u8 *mmio = rdrv->mmio_base;
     
     if (R100_IS_SET( CLIP ))
          return;
  
     /* 2d clip */
     r100_waitfifo( rdrv, rdev, 2 );
     if (rdev->dst_422) {
          r100_out32( mmio, SC_TOP_LEFT,
                      (clip->y1 << 16) | (clip->x1/2 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT,
                      ((clip->y2+1) << 16) | ((clip->x2+1)/2 & 0xffff) );
     } else {     
          r100_out32( mmio, SC_TOP_LEFT, 
                      (clip->y1 << 16) | (clip->x1 & 0xffff) );
          r100_out32( mmio, SC_BOTTOM_RIGHT,
                      ((clip->y2+1) << 16) | ((clip->x2+1) & 0xffff) );
     }
      
     /* 3d clip */
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, RE_TOP_LEFT, 
                 (clip->y1 << 16) | (clip->x1 & 0xffff) );
     r100_out32( mmio, RE_BOTTOM_RIGHT,
                 (clip->y2 << 16) | (clip->x2 & 0xffff) );
     
     rdev->clip = state->clip;
     
     R100_SET( CLIP );
}

#define R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v ) {        \
     r100_out32( (rdrv)->fb_base, (rdev)->yuv422_buffer,      \
                 PIXEL_YUY2( y, u, v ) );                     \
     r100_waitfifo( rdrv, rdev, 1 );                          \
     r100_out32( (rdrv)->mmio_base, PP_TXOFFSET_1,            \
                 (rdev)->fb_offset + (rdev)->yuv422_buffer ); \
}

void r100_set_drawing_color( R100DriverData *rdrv,
                             R100DeviceData *rdev,
                             CardState      *state )
{
     DFBColor color   = state->color;
     int      index   = state->color_index;
     __u32    color2d;
     __u32    color3d;
     int      y, u, v;

     if (R100_IS_SET( COLOR ) && R100_IS_SET( DRAWING_FLAGS ))
          return;

     color3d = PIXEL_ARGB( color.a, color.r,
                           color.g, color.b );
 
     switch (rdev->dst_format) {
          case DSPF_ALUT44:
               index |= (color.a & 0xf0);
          case DSPF_LUT8:
               color2d = index;
               color3d = PIXEL_RGB32( index, index, index );
               break;
          case DSPF_A8:
               color2d = color.a;
               color3d = (color.a << 24) | 0x00ffffff;
               break;
          case DSPF_RGB332:
               color2d = PIXEL_RGB332( color.r, color.g, color.b );
               break;
          case DSPF_ARGB2554:
               color2d = PIXEL_ARGB2554( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_ARGB4444:
               color2d = PIXEL_ARGB4444( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_ARGB1555:
               color2d = PIXEL_ARGB1555( color.a, color.r,
                                         color.g, color.b );
               break;
          case DSPF_RGB16:
               color2d = PIXEL_RGB16( color.r, color.g, color.b );
               break;
          case DSPF_RGB32:
               color2d = PIXEL_RGB32( color.r, color.g, color.b );
               break;
          case DSPF_ARGB:
               color2d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
          case DSPF_AiRGB:
               color2d = PIXEL_AiRGB( color.a, color.r,
                                      color.g, color.b );
               break;
          case DSPF_UYVY:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_UYVY( y, u, v );
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               color2d = PIXEL_YUY2( y, u, v );
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
               break;
          case DSPF_I420:
          case DSPF_YV12:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               rdev->y_cop  = PIXEL_ARGB( color.a, y, y, y );
               rdev->cb_cop = PIXEL_ARGB( color.a, u, u, u );
               rdev->cr_cop = PIXEL_ARGB( color.a, v, v, v );
               color3d = color2d = rdev->y_cop;
               break;
          default:
               D_BUG( "unexpected pixelformat" );
               color2d = 0;
               break;
     }
     
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( rdrv->mmio_base, DP_BRUSH_FRGD_CLR, color2d );
     r100_out32( rdrv->mmio_base, PP_TFACTOR_1, color3d );

     R100_SET( COLOR );
}

void r100_set_blitting_color( R100DriverData *rdrv,
                              R100DeviceData *rdev,
                              CardState      *state )
{
     DFBColor color   = state->color;
     __u32    color3d;
     int      y, u, v;
     
     if (R100_IS_SET( COLOR ) && R100_IS_SET( BLITTING_FLAGS ))
          return;

     if (state->blittingflags & DSBLIT_COLORIZE &&
         state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
          color.r = ((long) color.r * color.a / 255L);
          color.g = ((long) color.g * color.a / 255L);
          color.b = ((long) color.b * color.a / 255L);
     }

     switch (rdev->dst_format) {
          case DSPF_A8:
               color3d = (color.a << 24) | 0x00ffffff;
               break;
          case DSPF_I420:
          case DSPF_YV12: 
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               rdev->y_cop  = PIXEL_ARGB( color.a, y, y, y );
               rdev->cb_cop = PIXEL_ARGB( color.a, u, u, u );
               rdev->cr_cop = PIXEL_ARGB( color.a, v, v, v );
               color3d = rdev->y_cop;
               break;
          case DSPF_UYVY:
          case DSPF_YUY2:
               RGB_TO_YCBCR( color.r, color.g, color.b, y, u, v );
               R100_SET_YUV422_COLOR( rdrv, rdev, y, u, v );
          default:
               color3d = PIXEL_ARGB( color.a, color.r,
                                     color.g, color.b );
               break;
     }
     
     r100_waitfifo( rdrv, rdev, 1 );
     r100_out32( rdrv->mmio_base, PP_TFACTOR_0, color3d );
     
     R100_SET( COLOR );
}

void r100_set_src_colorkey( R100DriverData *rdrv,
                            R100DeviceData *rdev,
                            CardState      *state )
{
     volatile __u8 *mmio = rdrv->mmio_base;
     
     if (R100_IS_SET( SRC_COLORKEY ))
          return;
     
     rdev->src_key = state->src_colorkey;
     
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, CLR_CMP_CLR_SRC, rdev->src_key ); 
     r100_out32( mmio, CLR_CMP_MASK,    rdev->src_mask );    
     
     R100_SET( SRC_COLORKEY );
}

void
r100_set_blend_function( R100DriverData *rdrv,
                         R100DeviceData *rdev,
                         CardState      *state )
{
     volatile __u8 *mmio   = rdrv->mmio_base;
     __u32          sblend;
     __u32          dblend;
     
     if (R100_IS_SET( SRC_BLEND ) && R100_IS_SET( DST_BLEND ))
          return;

     sblend = r100SrcBlend[state->src_blend-1];
     dblend = r100DstBlend[state->dst_blend-1];

     if (!DFB_PIXELFORMAT_HAS_ALPHA( rdev->dst_format )) {
          switch (state->src_blend) {
               case DSBF_DESTALPHA:
                    sblend = SRC_BLEND_GL_ONE;
                    break;
               case DSBF_INVDESTALPHA:
                    sblend = SRC_BLEND_GL_ZERO;
                    break;
               default:
                    break;
          }
     }

     r100_waitfifo( rdrv, rdev, 1 ); 
     r100_out32( mmio, RB3D_BLENDCNTL, sblend | dblend );
     
     R100_SET( SRC_BLEND );
     R100_SET( DST_BLEND );
}

/* NOTES:
 * - We use texture unit 0 for blitting functions,
 *          texture unit 1 for drawing functions
 * - Default blend equation is ADD_CLAMP (A * B + C)
 */

void r100_set_drawingflags( R100DriverData *rdrv,
                            R100DeviceData *rdev,
                            CardState      *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->dp_gui_master_cntl;
     __u32          rb3d_cntl   = rdev->rb3d_cntl & ~DITHER_ENABLE;
     __u32          pp_cntl     = SCISSOR_ENABLE | TEX_BLEND_1_ENABLE;
     __u32          cblend      = COLOR_ARG_C_TFACTOR_COLOR;
     
     if (R100_IS_SET( DRAWING_FLAGS ))
          return;

     master_cntl |= GMC_SRC_DATATYPE_MONO_FG_LA | 
                    GMC_BRUSH_SOLID_COLOR       |
                    GMC_DP_SRC_SOURCE_MEMORY    |
                    GMC_CLR_CMP_CNTL_DIS;

     if (rdev->dst_422) {
          pp_cntl     |= TEX_1_ENABLE;
          cblend       = COLOR_ARG_C_T1_COLOR;
     }
     
     if (state->drawingflags & DSDRAW_BLEND) {
          rb3d_cntl   |= ALPHA_BLEND_ENABLE;
     }
     else if (rdev->dst_format == DSPF_A8) {
          cblend       = COLOR_ARG_C_TFACTOR_ALPHA;
     }

     if (state->drawingflags & DSDRAW_XOR) {
          rb3d_cntl   |= ROP_ENABLE;
          master_cntl |= GMC_ROP3_PATXOR;
     } else
          master_cntl |= GMC_ROP3_PATCOPY;
 
     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl );
     r100_out32( mmio, DP_CNTL, DST_X_LEFT_TO_RIGHT | DST_Y_TOP_TO_BOTTOM );
     
     r100_waitfifo( rdrv, rdev, 6 );
     r100_out32( mmio, RB3D_CNTL, rb3d_cntl );
     r100_out32( mmio, SE_CNTL, DIFFUSE_SHADE_FLAT  |
                                ALPHA_SHADE_FLAT    |
                                BFACE_SOLID         |
                                FFACE_SOLID         |
                                VTX_PIX_CENTER_OGL  |
                                ROUND_MODE_ROUND    |
				            ROUND_PREC_4TH_PIX );
     r100_out32( mmio, PP_CNTL, pp_cntl );
     r100_out32( mmio, PP_TXCBLEND_1, cblend );
     r100_out32( mmio, PP_TXABLEND_1, ALPHA_ARG_C_TFACTOR_ALPHA );
     r100_out32( mmio, SE_VTX_FMT, SE_VTX_FMT_XY );
     
     rdev->drawingflags = state->drawingflags;

     R100_SET  ( DRAWING_FLAGS );
     R100_UNSET( BLITTING_FLAGS );
}

void r100_set_blittingflags( R100DriverData *rdrv,
                             R100DeviceData *rdev,
                             CardState      *state )
{
     volatile __u8 *mmio        = rdrv->mmio_base;
     __u32          master_cntl = rdev->dp_gui_master_cntl;
     __u32          cmp_cntl    = 0;
     __u32          rb3d_cntl   = rdev->rb3d_cntl;
     __u32          se_cntl     = BFACE_SOLID        |
                                  FFACE_SOLID        |
                                  VTX_PIX_CENTER_OGL |
                                  ROUND_MODE_ROUND;
     __u32          pp_cntl     = SCISSOR_ENABLE    | 
                                  TEX_0_ENABLE      |
                                  TEX_BLEND_0_ENABLE;
     __u32          cblend      = COLOR_ARG_C_T0_COLOR;
     __u32          ablend      = ALPHA_ARG_C_T0_ALPHA;
     __u32          vtx_fmt     = SE_VTX_FMT_XY | SE_VTX_FMT_ST0;
     __u32          coord_fmt   = VTX_XY_PRE_MULT_1_OVER_W0 | 
                                  TEX1_W_ROUTING_USE_W0;
     
     if (R100_IS_SET( BLITTING_FLAGS ))
          return;
 
     if (rdev->accel == DFXL_TEXTRIANGLES) {
          se_cntl   |= DIFFUSE_SHADE_GOURAUD  |
                       ALPHA_SHADE_GOURAUD    |
                       SPECULAR_SHADE_GOURAUD |
                       FLAT_SHADE_VTX_LAST    |
                       ROUND_PREC_8TH_PIX;
          vtx_fmt   |= SE_VTX_FMT_W0 | SE_VTX_FMT_Z;
     }
     else {
          se_cntl   |= DIFFUSE_SHADE_FLAT |
                       ALPHA_SHADE_FLAT   |
                       ROUND_PREC_4TH_PIX;
          coord_fmt |= VTX_ST0_NONPARAMETRIC |
                       VTX_ST1_NONPARAMETRIC;
     }
    
     if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                 DSBLIT_BLEND_ALPHACHANNEL)) {
          if (state->blittingflags & DSBLIT_BLEND_COLORALPHA) {
               if (state->blittingflags & DSBLIT_BLEND_ALPHACHANNEL)
                    ablend = ALPHA_ARG_A_T0_ALPHA | ALPHA_ARG_B_TFACTOR_ALPHA;
               else
                    ablend = ALPHA_ARG_C_TFACTOR_ALPHA;
          }
          
          rb3d_cntl |= ALPHA_BLEND_ENABLE;
     }

     if (rdev->dst_format != DSPF_A8) {    
          if (state->blittingflags & DSBLIT_COLORIZE) {
               if (rdev->dst_422) {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (COLOR_ARG_C_T1_COLOR)
                             : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_T1_COLOR);

                    pp_cntl |= TEX_1_ENABLE;
               }
               else {
                    cblend = (rdev->src_format == DSPF_A8)
                             ? (COLOR_ARG_C_TFACTOR_COLOR)
                             : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_TFACTOR_COLOR);
               }
          }
          else if (state->blittingflags & DSBLIT_SRC_PREMULTCOLOR) {
               cblend = (rdev->src_format == DSPF_A8)
                        ? (COLOR_ARG_C_T0_ALPHA)
                        : (COLOR_ARG_A_T0_COLOR | COLOR_ARG_B_TFACTOR_ALPHA);
          }
     } /* DSPF_A8 */
     else {
          if (state->blittingflags & (DSBLIT_BLEND_COLORALPHA |
                                      DSBLIT_BLEND_ALPHACHANNEL))
               cblend = COLOR_ARG_C_TFACTOR_COLOR;
          else
               cblend = COLOR_ARG_C_T0_ALPHA;
     }
 
     if (state->blittingflags & DSBLIT_SRC_COLORKEY)
          cmp_cntl = SRC_CMP_EQ_COLOR | CLR_CMP_SRC_SOURCE;
     else
          master_cntl |= GMC_CLR_CMP_CNTL_DIS;

     if (state->blittingflags & DSBLIT_XOR) {
          master_cntl |= GMC_ROP3_PATXOR;
          rb3d_cntl   |= ROP_ENABLE; 
     } else
          master_cntl |= GMC_ROP3_SRCCOPY;

     r100_waitfifo( rdrv, rdev, 2 );
     r100_out32( mmio, CLR_CMP_CNTL, cmp_cntl );
     r100_out32( mmio, DP_GUI_MASTER_CNTL, master_cntl              |
                                           GMC_BRUSH_NONE           |
                                           GMC_SRC_DATATYPE_COLOR   |
                                           GMC_DP_SRC_SOURCE_MEMORY );
     
     r100_waitfifo( rdrv, rdev, 7 );
     r100_out32( mmio, RB3D_CNTL, rb3d_cntl );
     r100_out32( mmio, SE_CNTL, se_cntl );
     r100_out32( mmio, PP_CNTL, pp_cntl );
     r100_out32( mmio, PP_TXCBLEND_0, cblend );
     r100_out32( mmio, PP_TXABLEND_0, ablend );
     r100_out32( mmio, SE_VTX_FMT, vtx_fmt );
     r100_out32( mmio, SE_COORD_FMT, coord_fmt );
     
     rdev->blittingflags = state->blittingflags;
     
     R100_SET  ( BLITTING_FLAGS );
     R100_UNSET( DRAWING_FLAGS );
}
