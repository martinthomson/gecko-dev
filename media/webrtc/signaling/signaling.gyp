# Copyright (c) 2011 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
# Indent 2 spaces, no tabs.
#
#
# sip.gyp - a library for SIP
#

{
  'variables': {
    'chromium_code': 1,
  },

  'target_defaults': {
    'conditions': [
      ['moz_widget_toolkit_gonk==1', {
        'defines' : [
          'WEBRTC_GONK',
       ],
      }],
    ],
  },

  'targets': [

    #
    # ECC
    #
    {
      'target_name': 'ecc',
      'type': 'static_library',

      #
      # INCLUDES
      #
      'include_dirs': [
        '..',
        './src',
        './src/callcontrol',
        './src/common',
        './src/common/browser_logging',
        './src/common/time_profiling',
        './src/media',
        './src/media-conduit',
        './src/mediapipeline',
        './src/softphonewrapper',
        './src/peerconnection',
        './src/sdp/sipcc',
        '../../../xpcom/base',
        '../../../dom/base',
        '../../../content/media',
        '../../../media/mtransport',
        '../trunk',
        '../trunk/webrtc',
        '../trunk/webrtc/video_engine/include',
        '../trunk/webrtc/voice_engine/include',
        '../trunk/webrtc/modules/interface',
        '../../libyuv/include',
        '../../mtransport/third_party/nrappkit/src/util/libekr',
      ],

      #
      # DEPENDENCIES
      #
      'dependencies': [
      ],

      'export_dependent_settings': [
      ],


      #
      # SOURCES
      #
      'sources': [
        # Media Conduit
        './src/media-conduit/AudioConduit.h',
        './src/media-conduit/AudioConduit.cpp',
        './src/media-conduit/VideoConduit.h',
        './src/media-conduit/VideoConduit.cpp',
        './src/media-conduit/CodecStatistics.h',
        './src/media-conduit/CodecStatistics.cpp',
        './src/media-conduit/RunningStat.h',
        './src/media-conduit/GmpVideoCodec.cpp',
        './src/media-conduit/WebrtcGmpVideoCodec.cpp',
        # Common
        './src/common/CommonTypes.h',
        './src/common/csf_common.h',
        './src/common/NullDeleter.h',
        './src/common/Wrapper.h',
        './src/common/NullTransport.h',
        './src/common/YuvStamper.cpp',
        # Browser Logging
        './src/common/browser_logging/CSFLog.cpp',
        './src/common/browser_logging/CSFLog.h',
        './src/common/browser_logging/WebRtcLog.cpp',
        './src/common/browser_logging/WebRtcLog.h',
        # Browser Logging
        './src/common/time_profiling/timecard.c',
        './src/common/time_profiling/timecard.h',
        # PeerConnection
        './src/peerconnection/MediaStreamList.cpp',
        './src/peerconnection/MediaStreamList.h',
        './src/peerconnection/PeerConnectionCtx.cpp',
        './src/peerconnection/PeerConnectionCtx.h',
        './src/peerconnection/PeerConnectionImpl.cpp',
        './src/peerconnection/PeerConnectionImpl.h',
        './src/peerconnection/PeerConnectionMedia.cpp',
        './src/peerconnection/PeerConnectionMedia.h',
        # Media pipeline
        './src/mediapipeline/MediaPipeline.h',
        './src/mediapipeline/MediaPipeline.cpp',
        './src/mediapipeline/MediaPipelineFilter.h',
        './src/mediapipeline/MediaPipelineFilter.cpp',
         # SDP
         './src/sdp/sipcc/cpr_string.c',
         './src/sdp/sipcc/sdp_access.c',
         './src/sdp/sipcc/sdp_attr.c',
         './src/sdp/sipcc/sdp_attr_access.c',
         './src/sdp/sipcc/sdp_base64.c',
         './src/sdp/sipcc/sdp_config.c',
         './src/sdp/sipcc/sdp_main.c',
         './src/sdp/sipcc/sdp_token.c',
         './src/sdp/sipcc/sdp.h',
         './src/sdp/sipcc/sdp_base64.h',
         './src/sdp/sipcc/sdp_os_defs.h',
         './src/sdp/sipcc/sdp_private.h',
         './src/sdp/sipcc/sdp_utils.c',
         './src/sdp/sipcc/sdp_services_unix.c',

         # SDP Wrapper
         './src/sdp/Sdp.h',
         './src/sdp/SdpAttribute.h',
         './src/sdp/SdpAttributeList.h',
         './src/sdp/SdpMediaSection.h',
         './src/sdp/SipccSdp.h',
         './src/sdp/SipccSdpAttributeList.h',
         './src/sdp/SipccSdpAttributeList.cpp',
         './src/sdp/SipccSdpMediaSection.h',
         './src/sdp/SipccSdpParser.h',
         './src/sdp/SipccSdp.cpp',
         './src/sdp/SipccSdpMediaSection.cpp',
         './src/sdp/SipccSdpParser.cpp',

         # JSEP
         './src/jsep/JsepSessionImpl.cpp',
      ],

      #
      # DEFINES
      #

      'defines' : [
        'LOG4CXX_STATIC',
        '_NO_LOG4CXX',
        'USE_SSLEAY',
        '_CPR_USE_EXTERNAL_LOGGER',
        'WEBRTC_RELATIVE_PATH',
        'HAVE_WEBRTC_VIDEO',
        'HAVE_WEBRTC_VOICE',
        'HAVE_STDINT_H=1',
        'HAVE_STDLIB_H=1',
        'HAVE_UINT8_T=1',
        'HAVE_UINT16_T=1',
        'HAVE_UINT32_T=1',
        'HAVE_UINT64_T=1',
      ],

      'cflags_mozilla': [
        '$(NSPR_CFLAGS)',
        '$(NSS_CFLAGS)',
        '$(MOZ_PIXMAN_CFLAGS)',
      ],


      #
      # Conditionals
      #
      'conditions': [
        # hack so I can change the include flow for SrtpFlow
        ['build_with_mozilla==1', {
          'sources': [
            './src/mediapipeline/SrtpFlow.h',
            './src/mediapipeline/SrtpFlow.cpp',
          ],
          'include_dirs!': [
            '../trunk/webrtc',
          ],
          'include_dirs': [
            '../../../netwerk/srtp/src/include',
            '../../../netwerk/srtp/src/crypto/include',
          ],
        }],
        ['moz_webrtc_omx==1', {
          'sources': [
            './src/media-conduit/WebrtcOMXH264VideoCodec.cpp',
            './src/media-conduit/OMXVideoCodec.cpp',
          ],
          'include_dirs': [
            # hack on hack to re-add it after SrtpFlow removes it
            '../../webrtc/trunk/webrtc',
            '../../../content/media/omx',
            '../../../gfx/layers/client',
          ],
          'cflags_mozilla': [
            '-I$(ANDROID_SOURCE)/frameworks/av/include/media/stagefright',
            '-I$(ANDROID_SOURCE)/frameworks/av/include',
            '-I$(ANDROID_SOURCE)/frameworks/native/include/media/openmax',
            '-I$(ANDROID_SOURCE)/frameworks/native/include',
            '-I$(ANDROID_SOURCE)/frameworks/native/opengl/include',
          ],
          'defines' : [
            'MOZ_WEBRTC_OMX'
          ],
        }],
        ['build_for_test==0', {
          'defines' : [
            'MOZILLA_INTERNAL_API'
          ],
          'sources': [
            './src/peerconnection/WebrtcGlobalInformation.cpp',
            './src/peerconnection/WebrtcGlobalInformation.h',
          ],
        }],
        ['build_for_test!=0', {
          'include_dirs': [
            './test'
          ],
          'defines' : [
            'NO_CHROMIUM_LOGGING',
            'USE_FAKE_MEDIA_STREAMS',
            'USE_FAKE_PCOBSERVER'
          ],
        }],
        ['(OS=="linux") or (OS=="android")', {
          'include_dirs': [
          ],

          'defines': [
            'OS_LINUX',
            'SIP_OS_LINUX',
            '_GNU_SOURCE',
            'LINUX',
            'GIPS_VER=3510',
            'SECLIB_OPENSSL',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="win"', {
          'include_dirs': [
          ],
          'defines': [
            'OS_WIN',
            'SIP_OS_WINDOWS',
            'WIN32',
            'GIPS_VER=3480',
            'SIPCC_BUILD',
            'HAVE_WINSOCK2_H'
          ],

          'cflags_mozilla': [
          ],
        }],
        ['os_bsd==1', {
          'include_dirs': [
          ],
          'defines': [
            # avoiding pointless ifdef churn
            'SIP_OS_OSX',
            'OSX',
            'SECLIB_OPENSSL',
          ],

          'cflags_mozilla': [
          ],
        }],
        ['OS=="mac"', {
          'include_dirs': [
          ],
          'defines': [
            'OS_MACOSX',
            'SIP_OS_OSX',
            'OSX',
            '_FORTIFY_SOURCE=2',
          ],

          'cflags_mozilla': [
          ],
        }],
      ],
    },

#    #
#    # SIPCC
#    #
#    {
#      'target_name': 'sipcc',
#      'type': 'static_library',
#
#      #
#      # INCLUDES
#      #
#      'include_dirs': [
#        './src/common/browser_logging',
#        './src/common/time_profiling',
#        './src/sipcc/include',
#        './src/sipcc/core/includes',
#        './src/sipcc/cpr/include',
#        './src/sipcc/core/common',
#        './src/sipcc/core/sipstack/h',
#        './src/sipcc/core/ccapp',
#        './src/sipcc/core/sdp',
#        './src/sipcc/core/gsm/h',
#        './src/sipcc/plat/common',
#        '../../../media/mtransport',
#        '../../../dom/base',
#        '../../../netwerk/sctp/datachannel',
#      ],
#
#      #
#      # DEPENDENCIES
#      #
#      'dependencies': [
#      ],
#
#
#      'export_dependent_settings': [
#      ],
#
#
#      #
#      # SOURCES
#      #
#      'sources': [
#        # CPR
#        './src/sipcc/cpr/include/cpr.h',
#        './src/sipcc/cpr/include/cpr_assert.h',
#        './src/sipcc/cpr/include/cpr_debug.h',
#        './src/sipcc/cpr/include/cpr_errno.h',
#        './src/sipcc/cpr/include/cpr_in.h',
#        './src/sipcc/cpr/include/cpr_ipc.h',
#        './src/sipcc/cpr/include/cpr_locks.h',
#        './src/sipcc/cpr/include/cpr_memory.h',
#        './src/sipcc/cpr/include/cpr_rand.h',
#        './src/sipcc/cpr/include/cpr_socket.h',
#        './src/sipcc/cpr/include/cpr_stddef.h',
#        './src/sipcc/cpr/include/cpr_stdio.h',
#        './src/sipcc/cpr/include/cpr_stdlib.h',
#        './src/sipcc/cpr/include/cpr_string.h',
#        './src/sipcc/cpr/include/cpr_strings.h',
#        './src/sipcc/cpr/include/cpr_threads.h',
#        './src/sipcc/cpr/include/cpr_time.h',
#        './src/sipcc/cpr/include/cpr_types.h',
#        './src/sipcc/cpr/common/cpr_string.c',
#
#        # SDP
#        './src/sipcc/core/sdp/ccsdp.c',
#        './src/sipcc/core/sdp/sdp_access.c',
#        './src/sipcc/core/sdp/sdp_attr.c',
#        './src/sipcc/core/sdp/sdp_attr_access.c',
#        './src/sipcc/core/sdp/sdp_base64.c',
#        './src/sipcc/core/sdp/sdp_config.c',
#        './src/sipcc/core/sdp/sdp_main.c',
#        './src/sipcc/core/sdp/sdp_token.c',
#        './src/sipcc/core/sdp/sdp.h',
#        './src/sipcc/core/sdp/sdp_base64.h',
#        './src/sipcc/core/sdp/sdp_os_defs.h',
#        './src/sipcc/core/sdp/sdp_private.h',
#        './src/sipcc/core/sdp/sdp_utils.c',
#        './src/sipcc/core/sdp/sdp_services_unix.c',
#      ],
#
#      #
#      # DEFINES
#      #
#
#      'defines' : [
#      # CPR timers are needed by SIP, but are disabled for now
#      # to avoid the extra timer thread and stale cleanup code
#      #    'CPR_TIMERS_ENABLED',
#      ],
#
#      'cflags_mozilla': [
#        '$(NSPR_CFLAGS)',
#      ],
#
#      #
#      # OS SPECIFIC
#      #
#      'conditions': [
#        ['(OS=="android") or (OS=="linux")', {
#          'include_dirs': [
#          ],
#
#          'defines' : [
#            'SIP_OS_LINUX',
#            '_GNU_SOURCE',
#            'CPR_MEMORY_LITTLE_ENDIAN',
#            'NO_SOCKET_POLLING',
#            'USE_TIMER_SELECT_BASED',
#            'FULL_BUILD',
#            'STUBBED_OUT',
#            'USE_PRINTF'
#            'LINUX',
#          ],
#
#          'cflags_mozilla': [
#          ],
#        }],
#        ['OS=="android"', {
#          'sources': [
#            # SIPSTACK
#            './src/sipcc/core/sipstack/sip_platform_task.c',
#
#            # PLAT
#            './src/sipcc/plat/common/dns_utils.c',
#
#            # CPR
#            './src/sipcc/cpr/android/cpr_android_errno.c',
#            './src/sipcc/cpr/android/cpr_android_init.c',
#            './src/sipcc/cpr/android/cpr_android_socket.c',
#            './src/sipcc/cpr/android/cpr_android_stdio.c',
#            './src/sipcc/cpr/android/cpr_android_string.c',
#            './src/sipcc/cpr/android/cpr_android_threads.c',
#            './src/sipcc/cpr/android/cpr_android_timers_using_select.c',
#
#            './src/sipcc/cpr/android/cpr_assert.h',
#            './src/sipcc/cpr/android/cpr_android_align.h',
#            './src/sipcc/cpr/android/cpr_android_assert.h',
#            './src/sipcc/cpr/android/cpr_android_errno.h',
#            './src/sipcc/cpr/android/cpr_android_in.h',
#            './src/sipcc/cpr/android/cpr_android_private.h',
#            './src/sipcc/cpr/android/cpr_android_rand.h',
#            './src/sipcc/cpr/android/cpr_android_socket.h',
#            './src/sipcc/cpr/android/cpr_android_stdio.h',
#            './src/sipcc/cpr/android/cpr_android_string.h',
#            './src/sipcc/cpr/android/cpr_android_strings.h',
#            './src/sipcc/cpr/android/cpr_android_time.h',
#            './src/sipcc/cpr/android/cpr_android_timers.h',
#            './src/sipcc/cpr/android/cpr_android_tst.h',
#            './src/sipcc/cpr/android/cpr_android_types.h',
#          ],
#        }],
#        ['OS=="linux"', {
#          'sources': [
#            # SIPSTACK
#            './src/sipcc/core/sipstack/sip_platform_task.c',
#
#            # PLAT
#            './src/sipcc/plat/common/dns_utils.c',
#
#            # CPR
#            './src/sipcc/cpr/linux/cpr_linux_errno.c',
#            './src/sipcc/cpr/linux/cpr_linux_init.c',
#            './src/sipcc/cpr/linux/cpr_linux_socket.c',
#            './src/sipcc/cpr/linux/cpr_linux_stdio.c',
#            './src/sipcc/cpr/linux/cpr_linux_string.c',
#            './src/sipcc/cpr/linux/cpr_linux_threads.c',
#            './src/sipcc/cpr/linux/cpr_linux_timers_using_select.c',
#
#            './src/sipcc/cpr/linux/cpr_assert.h',
#            './src/sipcc/cpr/linux/cpr_linux_align.h',
#            './src/sipcc/cpr/linux/cpr_linux_assert.h',
#            './src/sipcc/cpr/linux/cpr_linux_errno.h',
#            './src/sipcc/cpr/linux/cpr_linux_in.h',
#            './src/sipcc/cpr/linux/cpr_linux_private.h',
#            './src/sipcc/cpr/linux/cpr_linux_rand.h',
#            './src/sipcc/cpr/linux/cpr_linux_socket.h',
#            './src/sipcc/cpr/linux/cpr_linux_stdio.h',
#            './src/sipcc/cpr/linux/cpr_linux_string.h',
#            './src/sipcc/cpr/linux/cpr_linux_strings.h',
#            './src/sipcc/cpr/linux/cpr_linux_time.h',
#            './src/sipcc/cpr/linux/cpr_linux_timers.h',
#            './src/sipcc/cpr/linux/cpr_linux_tst.h',
#            './src/sipcc/cpr/linux/cpr_linux_types.h',
#
#          ],
#        }],
#        ['OS=="win"', {
#          'include_dirs': [
#          ],
#
#          'sources': [
#            # SIPSTACK
#            './src/sipcc/core/sipstack/sip_platform_win32_task.c',
#
#            # PLAT
#            './src/sipcc/plat/win32/dns_utils.c',
#            './src/sipcc/plat/win32/mystub.c',
#            './src/sipcc/plat/win32/plat_api_stub.c',
#            './src/sipcc/plat/win32/plat_api_win.c',
#            './src/sipcc/plat/win32/StdAfx.h',
#
#            # CPR
#            './src/sipcc/cpr/win32/cpr_win_assert.h',
#            './src/sipcc/cpr/win32/cpr_win_debug.c',
#            './src/sipcc/cpr/win32/cpr_win_debug.h',
#            './src/sipcc/cpr/win32/cpr_win_defines.h',
#            './src/sipcc/cpr/win32/cpr_win_errno.c',
#            './src/sipcc/cpr/win32/cpr_win_errno.h',
#            './src/sipcc/cpr/win32/cpr_win_in.h',
#            './src/sipcc/cpr/win32/cpr_win_init.c',
#            './src/sipcc/cpr/win32/cpr_win_locks.c',
#            './src/sipcc/cpr/win32/cpr_win_locks.h',
#            './src/sipcc/cpr/win32/cpr_win_rand.c',
#            './src/sipcc/cpr/win32/cpr_win_rand.h',
#            './src/sipcc/cpr/win32/cpr_win_socket.c',
#            './src/sipcc/cpr/win32/cpr_win_socket.h',
#            './src/sipcc/cpr/win32/cpr_win_stdio.c',
#            './src/sipcc/cpr/win32/cpr_win_stdio.h',
#            './src/sipcc/cpr/win32/cpr_win_string.c',
#            './src/sipcc/cpr/win32/cpr_win_string.h',
#            './src/sipcc/cpr/win32/cpr_win_strings.h',
#            './src/sipcc/cpr/win32/cpr_win_threads.c',
#            './src/sipcc/cpr/win32/cpr_win_time.h',
#            './src/sipcc/cpr/win32/cpr_win_timers.c',
#            './src/sipcc/cpr/win32/cpr_win_timers.h',
#            './src/sipcc/cpr/win32/cpr_win_types.h',
#
#          ],
#
#          'defines' : [
#            'SIP_OS_WINDOWS',
#            'WIN32',
#            'SIPCC_BUILD',
#            'SDP_WIN32',
#            'STUBBED_OUT',
#            'EXTERNAL_TICK_REQUIRED',
#            'GIPS_VER=3480',
#          ],
#
#          'cflags_mozilla': [
#          ],
#
#        }],
#        ['OS=="mac" or os_bsd==1', {
#
#          'include_dirs': [
#          ],
#
#          'sources': [
#             './src/sipcc/cpr/darwin/cpr_darwin_string.c',
#             './src/sipcc/cpr/darwin/cpr_darwin_string.h',
#             './src/sipcc/cpr/darwin/cpr_darwin_strings.h',
#          ],
#
#
#          'conditions': [
#            ['OS=="mac"', {
#              'defines' : [
#                'SIP_OS_OSX',
#                '_POSIX_SOURCE',
#                'CPR_MEMORY_LITTLE_ENDIAN',
#                'NO_SOCKET_POLLING',
#                'USE_TIMER_SELECT_BASED',
#                'FULL_BUILD',
#                'STUBBED_OUT',
#                'USE_PRINTF',
#                '_DARWIN_C_SOURCE',
#                'NO_NSPR_10_SUPPORT',
#              ],
#            }],
#            ['os_bsd==1', {
#              'defines' : [
#                'SIP_OS_OSX',
#                'CPR_MEMORY_LITTLE_ENDIAN',
#                'NO_SOCKET_POLLING',
#                'USE_TIMER_SELECT_BASED',
#                'FULL_BUILD',
#                'STUBBED_OUT',
#                'USE_PRINTF',
#                'NO_NSPR_10_SUPPORT',
#              ],
#            }],
#          ],
#          'cflags_mozilla': [
#          ],
#        }],
#      ],
#
#    },
  ],
}

# Local Variables:
# tab-width:2
# indent-tabs-mode:nil
# End:
# vim: set expandtab tabstop=2 shiftwidth=2:
