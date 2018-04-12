/**********
This library is free software; you can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License as published by the
Free Software Foundation; either version 2.1 of the License, or (at your
option) any later version. (See <http://www.gnu.org/copyleft/lesser.html>.)

This library is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
more details.

You should have received a copy of the GNU Lesser General Public License
along with this library; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
**********/
// "liveMedia"
// Copyright (c) 2010 UPTTOP.COM, All rights reserved.
// A server demultiplexer for an AVI file
// C++ header

#ifndef _AVI_FILE_SERVER_DEMUX_HH
#define _AVI_FILE_SERVER_DEMUX_HH

#ifndef _SERVER_MEDIA_SESSION_HH
#include "ServerMediaSession.hh"
#endif
#ifndef _AVI_DEMUXED_ELEMENTARY_STREAM_HH
#include "AVIDemuxedElementaryStream.hh"
#endif

class AVIFileServerDemux: public Medium {
public:
  static AVIFileServerDemux*
  createNew(UsageEnvironment& env, char const* fileName, Boolean reuseFirstSource);

  ServerMediaSubsession* newAudioServerMediaSubsession(); // MPEG-1 or 2 audio
  ServerMediaSubsession* newVideoServerMediaSubsession(Boolean iFramesOnly = False,
						       double vshPeriod = 5.0
		       /* how often (in seconds) to inject a Video_Sequence_Header,
			  if one doesn't already appear in the stream */);
  ServerMediaSubsession* newAC3AudioServerMediaSubsession(); // AC-3 audio (from VOB)

  unsigned fileSize() const { return fFileSize; }
  float fileDuration() const { return fFileDuration; }
  int prepareAVIFile(UsageEnvironment& env,
						char const* fileName,
						unsigned& fileSize);

  AVIDemux* fSession0Demux;
  
private:
  AVIFileServerDemux(UsageEnvironment& env, char const* fileName,
			  Boolean reuseFirstSource);
      // called only by createNew();
  virtual ~AVIFileServerDemux();

private:
  friend class AVIDemuxedServerMediaSubsession;
  AVIDemuxedElementaryStream* newElementaryStream(unsigned clientSessionId,
						       char streamIndex);

private:
  char const* fFileName;
  unsigned fFileSize;
  float fFileDuration;
  Boolean fReuseFirstSource;

  AVIDemux* fLastCreatedDemux;
  u_int32_t fLastClientSessionId;
};

#endif
