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
// Copyright (c) 1996-2010 Live Networks, Inc.  All rights reserved.
// A MPEG 1 or 2 Elementary Stream, demultiplexed from a Program Stream
// C++ header

#ifndef _AVI_DEMUXED_ELEMENTARY_STREAM_HH
#define _AVI_DEMUXED_ELEMENTARY_STREAM_HH

#ifndef _AVI_DEMUX_HH
#include "AVIDemux.hh"
#endif

class AVIDemuxedElementaryStream: public FramedSource {
public:
  unsigned char mpegVersion() const { return fMPEGversion; }

  AVIDemux& sourceDemux() const { return fOurSourceDemux; }

private: // We are created only by a MPEG1or2Demux (a friend)
  AVIDemuxedElementaryStream(UsageEnvironment& env,
			      char streamIndex,
			      AVIDemux& sourceDemux);
  virtual ~AVIDemuxedElementaryStream();

private:
  // redefined virtual functions:
  virtual void doGetNextFrame();
  virtual void doStopGettingFrames();
  virtual char const* MIMEtype() const;
  virtual unsigned maxFrameSize() const;

private:
  static void afterGettingFrame(void* clientData,
				unsigned frameSize, unsigned numTruncatedBytes,
				struct timeval presentationTime,
				unsigned durationInMicroseconds);

  void afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
			  struct timeval presentationTime,
			  unsigned durationInMicroseconds);

private:
  char fOurStreamIndex;
  AVIDemux& fOurSourceDemux;
  char const* fMIMEtype;
  unsigned char fMPEGversion;

  friend class AVIDemux;
};

#endif
