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
// Copyright (c) 1996-2011 Live Networks, Inc.  All rights reserved.
// A simplified version of "MPEG4VideoStreamFramer" that takes only complete,
// discrete frames (rather than an arbitrary byte stream) as input.
// This avoids the parsing and data copying overhead of the full
// "MPEG4VideoStreamFramer".
// Implementation

#include "AVIVideoDiscreteFramer.hh"
#include "AVIDemuxedElementaryStream.hh"

AVIVideoDiscreteFramer*
AVIVideoDiscreteFramer::createNew(UsageEnvironment& env,
                                  FramedSource* inputSource)
{
    // Need to add source type checking here???  #####
    return new AVIVideoDiscreteFramer(env, inputSource);
}

AVIVideoDiscreteFramer
::AVIVideoDiscreteFramer(UsageEnvironment& env,
                         FramedSource* inputSource)
    : MPEG4VideoStreamDiscreteFramer(env, inputSource, False)
{
	AVIDemuxedElementaryStream *es = (AVIDemuxedElementaryStream *)inputSource;
	uint8_t *data;
	int size;
	
	es->sourceDemux().getConfigData(es->sourceDemux().getVideoIndex(), &data, &size);
	fNumConfigBytes = size;
	fConfigBytes = new unsigned char[size];
	memcpy(fConfigBytes, data, size);
	fProfileAndLevelIndication = fConfigBytes[4];
	if (fProfileAndLevelIndication == 0) {
		fProfileAndLevelIndication = 1; // Simple Profile/Level 1
	}
}

AVIVideoDiscreteFramer::~AVIVideoDiscreteFramer()
{
}

void AVIVideoDiscreteFramer::doGetNextFrame()
{
    // Arrange to read data (which should be a complete MPEG-4 video frame)
    // from our data source, directly into the client's input buffer.
    // After reading this, we'll do some parsing on the frame.
    fInputSource->getNextFrame(fTo, fMaxSize,
                               afterGettingFrame, this,
                               FramedSource::handleClosure, this);
}

void AVIVideoDiscreteFramer
::afterGettingFrame(void* clientData, unsigned frameSize,
                    unsigned numTruncatedBytes,
                    struct timeval presentationTime,
                    unsigned durationInMicroseconds)
{
    AVIVideoDiscreteFramer* source = (AVIVideoDiscreteFramer*)clientData;
    source->afterGettingFrame1(frameSize, numTruncatedBytes,
                               presentationTime, durationInMicroseconds);
}

void AVIVideoDiscreteFramer
::afterGettingFrame1(unsigned frameSize, unsigned numTruncatedBytes,
                     struct timeval presentationTime,
                     unsigned durationInMicroseconds)
{
	// Check that the first 4 bytes are a system code:
#if 0
	if (frameSize >= 4 && fTo[0] == 0 && fTo[1] == 0 && fTo[2] == 1) {
        unsigned i = 3;
        if (fTo[i] == 0xB0 || fTo[i] == 0x00) { // VISUAL_OBJECT_SEQUENCE_START_CODE
			if (fTo[i] == 0x00) {  //  some strange AVI files don't have VOS header
				fProfileAndLevelIndication = 0xF5; //  assume: Advanced Simple@L5
			}
			else {
				// The next byte is the "profile_and_level_indication":
            	if (frameSize >= 5) fProfileAndLevelIndication = fTo[4];
			}
			
            // The start of this frame - up to the first GROUP_VOP_START_CODE
            // or VOP_START_CODE - is stream configuration information.  Save this:
            for (i = 7; i < frameSize; ++i) {
                if ((fTo[i] == 0xB3 /*GROUP_VOP_START_CODE*/ ||
                     fTo[i] == 0xB6 /*VOP_START_CODE*/)
                    && fTo[i-1] == 1 && fTo[i-2] == 0 && fTo[i-3] == 0) {
                    break; // The configuration information ends here
                }
            }
			
            fNumConfigBytes = i < frameSize ? i-3 : frameSize;
            delete[] fConfigBytes;
            fConfigBytes = new unsigned char[fNumConfigBytes];
            for (unsigned j = 0; j < fNumConfigBytes; ++j) fConfigBytes[j] = fTo[j];
        }
    }
#endif

    // Complete delivery to the client:
    fPictureEndMarker = True; // Assume that we have a complete 'picture' here
    fFrameSize = frameSize;
    fNumTruncatedBytes = numTruncatedBytes;
    fPresentationTime = presentationTime;
    fDurationInMicroseconds = durationInMicroseconds;
    afterGetting(this);
}

