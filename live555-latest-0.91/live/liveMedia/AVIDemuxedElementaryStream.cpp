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
// Copyright (c) 2010 UPTTOP.COM  All rights reserved.
// AVI Stream, demultiplexed from an AVI File
// Implementation

#include "AVIDemuxedElementaryStream.hh"

////////// AVIDemuxedElementaryStream //////////

AVIDemuxedElementaryStream::
AVIDemuxedElementaryStream(UsageEnvironment& env, char streamIndex,
						   AVIDemux& sourceDemux)
						   : FramedSource(env),
						   fOurStreamIndex(streamIndex), fOurSourceDemux(sourceDemux), fMPEGversion(0) 
{
	// Set our MIME type string for known media types:
	if (streamIndex == sourceDemux.fAudioIndex) {
		fMIMEtype = "audio/MPEG";
	} else if (streamIndex == sourceDemux.fVideoIndex) {
		fMIMEtype = "video/MPEG";
	} else {
		fMIMEtype = MediaSource::MIMEtype();
	}
}

AVIDemuxedElementaryStream::~AVIDemuxedElementaryStream() {
//	fOurSourceDemux.noteElementaryStreamDeletion(this);
}

void AVIDemuxedElementaryStream::doGetNextFrame() {
	fOurSourceDemux.getNextFrame(fOurStreamIndex, fTo, fMaxSize,
		afterGettingFrame, this,
		handleClosure, this);
}

void AVIDemuxedElementaryStream::doStopGettingFrames() {
	fOurSourceDemux.stopGettingFrames(fOurStreamIndex);
}

char const* AVIDemuxedElementaryStream::MIMEtype() const {
	return fMIMEtype;
}

unsigned AVIDemuxedElementaryStream::maxFrameSize() const {
	return 512*1024;
	// because the MPEG spec allows for PES packets as large as
	// (6 + 65535) bytes (header + data)
}

void AVIDemuxedElementaryStream
::afterGettingFrame(void* clientData,
					unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds) {
		AVIDemuxedElementaryStream* stream
			= (AVIDemuxedElementaryStream*)clientData;
		stream->afterGettingFrame1(frameSize, numTruncatedBytes,
			presentationTime, durationInMicroseconds);
}

void AVIDemuxedElementaryStream
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
struct timeval presentationTime,
	unsigned durationInMicroseconds) {
		fFrameSize = frameSize;
		fNumTruncatedBytes = numTruncatedBytes;
		fPresentationTime = presentationTime;
		fDurationInMicroseconds = durationInMicroseconds;

		FramedSource::afterGetting(this);
}
