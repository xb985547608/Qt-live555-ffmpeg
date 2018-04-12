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
// Implementation

#include "AVIFileServerDemux.hh"
#include "AVIDemuxedServerMediaSubsession.hh"
#include "ByteStreamFileSource.hh"

AVIFileServerDemux*
AVIFileServerDemux::createNew(UsageEnvironment& env, char const* fileName,
								   Boolean reuseFirstSource) {
	return new AVIFileServerDemux(env, fileName, reuseFirstSource);
}


AVIFileServerDemux
::AVIFileServerDemux(UsageEnvironment& env, char const* fileName,
						  Boolean reuseFirstSource)
						  : Medium(env),fSession0Demux(NULL),
						  fReuseFirstSource(reuseFirstSource),
						  fLastCreatedDemux(NULL), fLastClientSessionId(~0) {
	fFileName = strDup(fileName);
	//fFileDuration = AVIFileDuration(env, fileName, fFileSize);
	prepareAVIFile(env, fileName, fFileSize);
	fFileDuration = fSession0Demux->getDuration();
}

AVIFileServerDemux::~AVIFileServerDemux() {
	Medium::close(fSession0Demux);
	delete[] (char*)fFileName;
}

ServerMediaSubsession*
AVIFileServerDemux::newAudioServerMediaSubsession() {
	return AVIDemuxedServerMediaSubsession::createNew(*this, fSession0Demux->getAudioIndex(), fReuseFirstSource);
}

ServerMediaSubsession*
AVIFileServerDemux::newVideoServerMediaSubsession(Boolean iFramesOnly,
													   double vshPeriod) {
	return AVIDemuxedServerMediaSubsession::createNew(*this, fSession0Demux->getVideoIndex(), fReuseFirstSource,
							 iFramesOnly, vshPeriod);
}


AVIDemuxedElementaryStream*
AVIFileServerDemux::newElementaryStream(unsigned clientSessionId,
											 char streamIndex) {
	/*AVIDemux* demuxToUse;
	if (clientSessionId == 0) {
		// 'Session 0' is treated especially, because its audio & video streams
		// are created and destroyed one-at-a-time, rather than both streams being
		// created, and then (later) both streams being destroyed (as is the case
		// for other ('real') session ids).  Because of this, a separate demux is
		// used for session 0, and its deletion is managed by us, rather than
		// happening automatically.
		if (fSession0Demux == NULL) {
			// Open our input file as a 'byte-stream file source':
		//	ByteStreamFileSource* fileSource
		//		= ByteStreamFileSource::createNew(envir(), fFileName);
		//	if (fileSource == NULL) return NULL;
			fSession0Demux = AVIDemux::createNew(envir(), fFileName);
			fSession0Demux->prepare();
		}
		demuxToUse = fSession0Demux;
	} else {
		// First, check whether this is a new client session.  If so, create a new
		// demux for it:
		if (clientSessionId != fLastClientSessionId) {
			// Open our input file as a 'byte-stream file source':
		//	ByteStreamFileSource* fileSource
		//		= ByteStreamFileSource::createNew(envir(), fFileName);
		//	if (fileSource == NULL) return NULL;
			
			fLastCreatedDemux = AVIDemux::createNew(envir(), fFileName);
			fLastCreatedDemux->prepare();
			// Note: We tell the demux to delete itself when its last
			// elementary stream is deleted.
			fLastClientSessionId = clientSessionId;
			// Note: This code relies upon the fact that the creation of streams for
			// different client sessions do not overlap - so one "MPEG1or2Demux" is used
			// at a time.
		}
		demuxToUse = fLastCreatedDemux;
	}
	
	if (demuxToUse == NULL) return NULL; // shouldn't happen*/
	
	return fSession0Demux->newElementaryStream(streamIndex);
}

/*
static Boolean getMPEG1or2TimeCode(FramedSource* dataSource,
								   AVIDemux& parentDemux,
								   Boolean returnFirstSeenCode,
								   float& timeCode); // forward
*/

int AVIFileServerDemux::prepareAVIFile(UsageEnvironment& env,
											   char const* fileName,
											   unsigned& fileSize) {
	
	fileSize = 0; // ditto
	
	do {
		// Open the input file as a 'byte-stream file source':
		//ByteStreamFileSource* fileSource = ByteStreamFileSource::createNew(env, fileName);
		//if (fileSource == NULL) break;
		//dataSource = fileSource;
		
		//fileSize = (unsigned)(fileSource->fileSize());
		//if (fileSize == 0) break;
		
		// Create a AVI demultiplexor that reads from that source.
		fSession0Demux = AVIDemux::createNew(env, fileName);
		if (fSession0Demux == NULL) break;

		fSession0Demux->prepare();
		
	} while (0);

	return 0;
}

//static void afterPlayingDummySink(DummySink* sink); // forward
//static float computeSCRTimeCode(AVIDemux::SCR const& scr); // forward

/*
static Boolean getMPEG1or2TimeCode(FramedSource* dataSource,
								   AVIDemux& parentDemux,
								   Boolean returnFirstSeenCode,
								   float& timeCode) {
	// Start reading through "dataSource", until we see a SCR time code:
//	parentDemux.lastSeenSCR().isValid = False;
	UsageEnvironment& env = dataSource->envir(); // alias
	DummySink sink(parentDemux, returnFirstSeenCode);
	sink.startPlaying(*dataSource,
		(MediaSink::afterPlayingFunc*)afterPlayingDummySink, &sink);
	env.taskScheduler().doEventLoop(&sink.watchVariable);
	
//	timeCode = computeSCRTimeCode(parentDemux.lastSeenSCR());
	return parentDemux.lastSeenSCR().isValid;
}
*/

