//  Mach1 Spatial SDK
//  Copyright © 2017-2021 Mach1. All rights reserved.

/*
 Order of Operations:
 1. Setup Input and Output formats (and paths)
 2. Call `processConversionPath()` to setup the conversion for processing
 3. Use `setSpatialDownmixer()` & `getSpatialDownmixerPossibility()` to downmix content to Mach1Horizon if top/bottom
    difference is less than correlation threshold
    Note: Afterwards reinitizalize setup of Input and Output formats
 4. Call `processConversion()` to execute the conversion and return coeffs per buffer/sample per channel
 5. Apply to buffer/samples per channel in file rendering or audio mixer
 */

#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <stdlib.h>
#include <cstring>
#include <iostream>
#include <vector>
#include <sstream>
#include <fstream>
#include <string>

#include "Mach1Transcode.h"
#include "Mach1AudioTimeline.h"

#include "sndfile.hh"
#include "CmdOption.h"
#include "ADMParse.h"
#include "yaml/Yaml.hpp"
#include "xml/pugixml.hpp"
#include "bw64/bw64.hpp"
#include "chunks.h"

std::vector<Mach1AudioObject> audioObjects;
std::vector<Mach1Point3D> keypoints;

Mach1Point3D* callbackPointsSampler(long long sample, int& n) {
    keypoints.resize(audioObjects.size());
    
    for (int i = 0; i < audioObjects.size(); i++) {
        std::vector<Mach1KeyPoint> kp = audioObjects[i].getKeyPoints();

        keypoints[i] = kp[0].point;
        for (int j = 0; j < kp.size(); j++) {
            if (kp[j].sample >= sample) {
                keypoints[i] = kp[j].point;
                continue;
            }
        }
    }

    n = keypoints.size();
    return keypoints.data();
}

using namespace std;

vector<string> &split(const string &s, char delim, vector<string> &elems) {
	stringstream ss(s);
	string item;
	while (getline(ss, item, delim)) {
		elems.push_back(item);
	}
	return elems;
}

string convertToString(char* a, int size) {
	int i;
	string s = "";
	for (i = 0; i < size; i++) {
		s = s + a[i];
	}
	return s;
}

void printHelp() {
	cout << "m1-transcode -- command line mach1 format conversion tool" << std::endl;
	cout << "ambisonics in collaboration with VVAudio: http://www.vvaudio.com/ " << std::endl;
	cout << std::endl;
	cout << "usage: fmtconv -in-file test_s8.wav -in-fmt M1Spatial -out-file test_b.wav -out-fmt ACNSN3D -out-file-chans 0" << std::endl;
	cout << std::endl;
	cout << "  -help                 - list command line options" << std::endl;
	cout << "  -in-file  <filename>  - input file: put quotes around sets of files" << std::endl;
	cout << "  -in-fmt   <fmt>       - input format: see supported formats below" << std::endl;
    cout << "  -in-json  <json>      - input json: for input custom json Mach1Transcode templates" << std::endl;
	cout << "  -out-file <filename>  - output file. full name for single file or name stem for file sets" << std::endl;
	cout << "  -out-fmt  <fmt>       - output format: see supported formats below" << std::endl;
    cout << "  -out-json  <json>     - output json: for output custom json Mach1Transcode templates" << std::endl;
	cout << "  -out-file-chans <#>   - output file channels: 1, 2 or 0 (0 = multichannel)" << std::endl;
	cout << "  -normalize            - two pass normalize absolute peak to zero dBFS" << std::endl;
	cout << "  -master-gain <#>      - final output gain in dB like -3 or 2.3" << std::endl;
	cout << "  -lfe-sub <#>          - indicates channel(s) to be filtered and treated as LFE/SUB, delimited by ',' for multiple channels" << std::endl;
	cout << "  -spatial-downmix <#>  - compare top vs. bottom of the input soundfield, if difference is less than the set threshold (float) output format will be Mach1 Horizon" << std::endl;
	cout << "  -extract-metadata     - export any detected XML metadata into separate text file" << std::endl;
	cout << std::endl;
	cout << "  Formats Supported:" << std::endl;
    cout << "    M1Horizon (Mach1 Horizon / Quad) - L R Ls Rs" << std::endl;
	cout << "    M1Horizon+S (Mach1 Horizon / Quad) - L R Ls Rs StereoL StereoR" << std::endl;
	cout << "    M1HorizonPairs (Mach1 Horizon / Quad-Binaural) - FrontPair, LeftPair, RearPair, RightPair" << std::endl;
	cout << "    M1Spatial (Mach1 Spatial) - Upper L R Ls Rs, Lower L R Ls Rs" << std::endl;
	cout << "    M1Spatial+S (Mach1 Spatial) - Upper L R Ls Rs, Lower L R Ls Rs, StereoL StereoR" << std::endl;
	cout << "    M1SpatialPairs (Mach1 Spatial Pairs) - Upper front, left, rear, right, pairs, then lower same" << std::endl;
	cout << "    M1SpatialFaces - Fc, Lc, Rc, Bc, Tc, Bc" << std::endl;
    cout << "    2.0_M                              - L & R spatialized" << std::endl;
    cout << "    2.0_C                              - L & R spatialized, forward focus" << std::endl;
    cout << "    3.0_LCR                            - L & R spatialized with C mono" << std::endl;
	cout << "    5.0_M                              - L C R Ls Rs" << std::endl;
    cout << "    5.0_C                              - L C R Ls Rs, cinema forward focus" << std::endl;
    cout << "    5.0_S                              - L C R Ls Rs, 110 degree surround sides & cinema forward focus" << std::endl;
	cout << "    5.1_M (Pro Tools default / C|24)   - L C R Ls Rs LFE" << std::endl;
	cout << "    5.1_C (Pro Tools default / C|24)   - L C R Ls Rs LFE, forward focus" << std::endl;
	cout << "    5.1_M (SMPTE/ITU for AC3)          - L R C LFE Ls Rs" << std::endl;
	cout << "    5.1_M (DTS)                        - L R Ls Rs C LFE" << std::endl;
    cout << "    5.0.2_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr FLts FRts BLts BRts" << std::endl;
    cout << "    5.1.2_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr FLts FRts BLts BRts" << std::endl;
    cout << "    5.1.4_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr FLts FRts BLts BRts" << std::endl;
    cout << "    5.0.4_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr FLts FRts BLts BRts" << std::endl;
	cout << "    6.0_M                              - L C R Ls Rs Cs" << std::endl;
    cout << "    7.0_C (Pro Tools default)          - L C R Lss Rss Lsr Rsr, forward focus" << std::endl;
	cout << "    7.1_M (Pro Tools default)          - L C R Lss Rss Lsr Rsr LFE" << std::endl;
	cout << "    7.1_C (Pro Tools default)          - L C R Lss Rss Lsr Rsr LFE, forward focus" << std::endl;
    cout << "    7.0_C (SMPTE)                      - L Lc C Rc R Ls Rs" << std::endl;
	cout << "    7.1_C (SMPTE)                      - L Lc C Rc R Ls Rs LFE" << std::endl;
	cout << "    7.1.2_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr LFE Lts Rts" << std::endl;
	cout << "    7.0.2_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr Lts Rts" << std::endl;
	cout << "    7.1.4_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr LFE FLts FRts BLts BRts" << std::endl;
	cout << "    7.0.4_M (Film / Pro Tools default) - L C R Lss Rss Lsr Rsr FLts FRts BLts BRts" << std::endl;
    cout << "    7.0.2_M (SMPTE)                    - L R C Lss Rss Lsr Rsr Lts Rts" << std::endl;
	cout << "    7.1.2_M (SMPTE)                    - L R C LFE Lss Rss Lsr Rsr Lts Rts" << std::endl;
    cout << "    7.1.2_C (Dolby Atmos channelbed)   - L R C LFE Lss Rss Lsr Rsr Lts Rts [ADM Metadata]" << std::endl;
    cout << "    16.0_M                             - 16 channel Surround 3D layout" << std::endl;
	cout << "    ACNSN3D                            - 1st order B-format, ACN order and SN3D weighting" << std::endl;
	cout << "    FuMa                               - 1st order B-format, Furse-Malham order and weighting" << std::endl;
	cout << "    TBE                                - W, X, Y, Z, U, V, T, S" << std::endl;
	cout << "    ACNSN3DO2A                         - 2nd order B-format, AmbiX ACN order and SN3D weighting" << std::endl;
	cout << "    FuMaO2A                            - 2nd order B-format, Furse-Malham order and weighting, W, Y, Z, X, V, T, R, S, U" << std::endl;
	cout << "    ACNSN3DO3A                         - 16 channel AmbiX" << std::endl;
	cout << "    FuMaO3A                            - 3rd order B-format, W, Y, Z, X, V, T, R, S, U, Q, O, M, K, L, N, P" << std::endl;
    cout << "    ACNSN3DmaxRE1oa                    - 1st order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE2oa                    - 2nd order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE3oa                    - 3rd order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE4oa                    - 4th order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE5oa                    - 5th order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE6oa                    - 6th order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
    cout << "    ACNSN3DmaxRE7oa                    - 7th order, AmbiX ACN order and SN3D-maxRE from IEM AllRAD" << std::endl;
	cout << std::endl;

}

struct audiofileInfo {
    int format;
    int sampleRate;
    int numberOfChannels;
    float duration;
};

audiofileInfo printFileInfo(SndfileHandle file) {
    audiofileInfo inputFileInfo;
    cout << "Sample Rate:        " << file.samplerate() << std::endl;
    inputFileInfo.sampleRate = file.samplerate();
    int format = file.format() & 0xffff;
    if (format == SF_FORMAT_PCM_16) cout << "Bit Depth:          16" << std::endl;
    if (format == SF_FORMAT_PCM_24) cout << "Bit Depth:          24" << std::endl;
    if (format == SF_FORMAT_FLOAT)  cout << "Bit Depth:          32" << std::endl;
    inputFileInfo.format = format;
    cout << "Channels:           " << file.channels() << std::endl;
    inputFileInfo.numberOfChannels = file.channels();
    cout << "Length (sec):       " << (float)file.frames() / (float)file.samplerate() << std::endl;
    inputFileInfo.duration = (float)file.frames() / (float)file.samplerate();
    cout << std::endl;
    
    return inputFileInfo;
}

std::string getTimecode(const char* admString, float duration) {
    // Used to find duration in time of input file
    // to correctly edit the ADM metadata and add the appropriate
    // `end` and `duration` times.
    std::string s(admString);
    std::string searchString("hh:mm:ss.fffff");
    size_t pos = s.find(searchString);
    
    int seconds, minutes, hours;
    std::string hoursString, minutesString, secondsString;
    seconds = duration;
    minutes = seconds / 60;
    hours = minutes / 60;
    
    if ((int)hours < 100) {
        hoursString = (int(hours) < 10) ? "0" + std::to_string(int(hours)) : std::to_string(int(hours));
    } else {
        // file duration too long?
        // TODO: handle case for when input is over 99 hours
    }
    minutesString = (int(minutes%60) < 10) ? "0" + std::to_string(int(minutes%60)) : std::to_string(int(minutes%60));
    secondsString = (int((seconds+1)%60) < 10) ? "0" + std::to_string(int((seconds+1)%60)) : std::to_string(int((seconds+1)%60));
    
    std::vector<size_t> positions;
    // Repeat till end is reached
    while(pos != std::string::npos){
        // Add position to the vector
        positions.push_back(pos);
        // Get the next occurrence from the current position
        pos = s.find(searchString, pos + searchString.size());
    }

    for (size_t pos : positions){
        s.replace(pos, searchString.length(), hoursString+":"+minutesString+":"+secondsString+".00000");
    }

    cout << "Detected Duration:  " << duration << std::endl;
    cout << "Duration Timecode:  " << hoursString << ":" << minutesString << ":" << secondsString << ".00000" << std::endl;
    
    return s;
}

// ---------------------------------------------------------
#define BUFFERLEN 512

class SndFileWriter {
    std::unique_ptr<bw64::Bw64Writer> outBw64;
    SndfileHandle outSnd;
    int channels;
    
    enum SNDFILETYPE {
        SNDFILETYPE_BW64,
        SNDFILETYPE_SND
    } type;

public:
    void open(std::string outfilestr, int sampleRate, int channels, int format) {
        outSnd = SndfileHandle(outfilestr, SFM_WRITE, format, channels, (int)sampleRate);
        this->channels = channels;
        type = SNDFILETYPE_SND;
    }

    void open(std::string outfilestr, int channels, bw64::ChnaChunk chnaChunkAdm, bw64::AxmlChunk axmlChunkAdm) {
        // TODO: make variable of samplerate and bitdepth based on input
        outBw64 = bw64::writeFile(outfilestr, channels, 48000u, 24u, std::make_shared<bw64::ChnaChunk>(chnaChunkAdm), std::make_shared<bw64::AxmlChunk>(axmlChunkAdm));
        this->channels = channels;
        type = SNDFILETYPE_BW64;
    }

    bool isOpened() {
        if (type == SNDFILETYPE_SND) {
            return outSnd.error() == 0;
        } else {
            return true;
        }
    }

    void setClip() {
        if (type == SNDFILETYPE_SND) {
            outSnd.command(SFC_SET_CLIPPING, NULL, SF_TRUE);
        }
    }

    void printInfo() {
        if (type == SNDFILETYPE_SND) {
            printFileInfo(outSnd);
        }
    }

    void setString(int str_type, const char* str) {
        if (type == SNDFILETYPE_SND) {
            outSnd.setString(str_type, str);
        }
    }

    void write(float* buf, int frames) {
        if (type == SNDFILETYPE_SND) {
            outSnd.write(buf, frames*channels);
        } else {
            outBw64->write(buf, frames);
            outBw64->framesWritten();
        }
    }
};

void parseFile(SndfileHandle infile, int channels) {
	float buf[BUFFERLEN];
	sf_count_t frames;
	int k, m, readcount;

	frames = BUFFERLEN / channels;
	while ((readcount = infile.readRaw(buf, frames)) > 0) {
		for (k = 0; k < readcount; k++) {
			for (m = 0; m < channels; m++) {
				// search here for xml
				//cout << "Buffer: " << buf [k * channels + m];
				//cout << std::endl;
			}
		}
	};
}

int main(int argc, char* argv[]) {
    Mach1AudioTimeline m1audioTimeline;
    Mach1Transcode m1transcode;
    m1transcode.setCustomPointsSamplerCallback(callbackPointsSampler);
    
	// locals for cmd line parameters
	bool fileOut = false;
    bool useAudioTimeline = false; // adm, atmos formats
	//TODO: inputGain = 1.0f; // in level, not db
	float masterGain = 1.0f; // in level, not dB
	bool normalize = false;
    char* infolder = NULL;
	char* infilename = NULL;
	char* inFmtStr = NULL;
	int inFmt;
	char* outfilename = NULL;
	std::string md_outfilename = "";
	char* outFmtStr = NULL;
    int outFmt;
	int outFileChans;
	int channels;
	bool spatialDownmixerMode = false;
	float corrThreshold = 0.0;
	std::vector<int> subChannelIndices;
	bool processSubs = false;
	bool extractMetadata = false;
	ADMParse admParse; // Reading ADM data

	sf_count_t totalSamples;
	long sampleRate;

	// multiplexed buffers
	float fileBuffer[Mach1TranscodeMAXCHANS * BUFFERLEN];

	// process buffers
	float inBuffers[Mach1TranscodeMAXCHANS][BUFFERLEN];
	float *inPtrs[Mach1TranscodeMAXCHANS];
	float outBuffers[Mach1TranscodeMAXCHANS][BUFFERLEN];
	float *outPtrs[Mach1TranscodeMAXCHANS];
	for (int i = 0; i < Mach1TranscodeMAXCHANS; i++) {
		inPtrs[i] = inBuffers[i];
		outPtrs[i] = outBuffers[i];
	}

	//=================================================================
	// read command line parameters
	//

	char *pStr;
	if (cmdOptionExists(argv, argv + argc, "-h")
		|| cmdOptionExists(argv, argv + argc, "-help")
		|| cmdOptionExists(argv, argv + argc, "--help")
		|| argc == 1)
	{
		printHelp();
		return 0;
	}
	pStr = getCmdOption(argv, argv + argc, "-normalize");
	if (pStr != NULL)
	{
		normalize = true;
	}
	pStr = getCmdOption(argv, argv + argc, "-master-gain");
	if (pStr != NULL)
	{
		masterGain = (float)atof(pStr); // still in dB
		masterGain = m1transcode.db2level(masterGain);
	}
	pStr = getCmdOption(argv, argv + argc, "-lfe-sub");
	/*
	 Submit channel index int(s) with commas as delimiters
	 Example: -lfe-sub 3,7
	 Indicates channels 4 and 8 as sub/LFE channels
	 */
	if (pStr && (strlen(pStr) > 0))
	{
		processSubs = true;
		char* lfeIndices = strtok(pStr, ",");
		while (lfeIndices) {
			//pushback int safe only
			subChannelIndices.push_back(stoi(lfeIndices));
			lfeIndices = strtok(NULL, ",");
		}
	}
	// flag for extracting metadata to a separate text file
	pStr = getCmdOption(argv, argv + argc, "-extract-metadata");
	if (pStr != NULL)
	{
		extractMetadata = true;
	}
	/*
	 flag for auto Mach1 Spatial downmixer
	 compares top/bottom to downmix to Horizon
	 TODO: scale to other formats
	 */
	pStr = getCmdOption(argv, argv + argc, "-spatial-downmix");
	if (pStr != NULL)
	{
		spatialDownmixerMode = true;
		corrThreshold = atof(pStr);
	}
	if (spatialDownmixerMode && (corrThreshold < 0.0 || corrThreshold > 1.0))
	{
		cout << "Please use 0.0 to 1.0 range for correlation threshold" << std::endl;
		return -1;
	}
	// input file name and format
	pStr = getCmdOption(argv, argv + argc, "-in-file");
	if (pStr && (strlen(pStr) > 0))
	{
		infilename = pStr;
	}
	else
	{
		cerr << "Please specify an input file" << std::endl;
		return -1;
	}
	pStr = getCmdOption(argv, argv + argc, "-in-fmt");
	if (pStr && (strlen(pStr) > 0))
    {
		inFmtStr = pStr;
        
        if (strcmp(inFmtStr, "ADM") == 0) {
            m1transcode.setInputFormat(m1transcode.getFormatFromString("CustomPoints"));
            m1audioTimeline.parseADM(infilename);
            useAudioTimeline = true;
        } else if (strcmp(inFmtStr, "Atmos") == 0) {
            char* pStr = getCmdOption(argv, argv + argc, "-in-file-meta");
            if (pStr && (strlen(pStr) > 0)) {
                m1transcode.setInputFormat(m1transcode.getFormatFromString("CustomPoints"));
                m1audioTimeline.parseAtmos(infilename, pStr);
                useAudioTimeline = true;
            } else {
                cerr << "Please specify an input meta file" << std::endl;
                return -1;
            }
        } else if (strcmp(inFmtStr, "TTPoints") == 0) {
			pStr = getCmdOption(argv, argv + argc, "-in-json");
			if (pStr && (strlen(pStr) > 0))
            {
                std::ifstream file(pStr);
                std::string strJson((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				m1transcode.setInputFormatCustomPointsJson((char*)strJson.c_str());
			}
        } else {
            bool foundInFmt = false;
            inFmt = m1transcode.getFormatFromString(inFmtStr);
            if (inFmt > 1) { // if format int is 0 or -1 (making it invalid)
                foundInFmt = true;
            }
        }
	} else {
		cout << "Please select a valid input format" << std::endl;
		return -1;
	}
    
    // input folder
    if (useAudioTimeline) {
        pStr = getCmdOption(argv, argv + argc, "-in-folder");
        if (pStr && (strlen(pStr) > 0)) {
            infolder = pStr;
        } else {
            cerr << "Please specify an input folder for audio files" << std::endl;
            return -1;
        }
    }

	// output file name and format
	pStr = getCmdOption(argv, argv + argc, "-out-file");
	if (pStr && (strlen(pStr) > 0))
	{
		fileOut = true;
		outfilename = pStr;
		std::string FileExt = ".txt";
		int outfilename_size = strlen(outfilename);
		md_outfilename = convertToString(outfilename, outfilename_size);
		std::string Path, FileName;
		std::string::size_type found = md_outfilename.find_last_of(".");
		// if we found one of this symbols
		if (found != std::string::npos) {
			// path will be all symbols before found position
			Path = md_outfilename.substr(0, found);
		}
		else { // if we not found '.', path is empty
			Path.clear();
		}
		md_outfilename += FileExt;
	}
	pStr = getCmdOption(argv, argv + argc, "-out-fmt");
	if (pStr && (strlen(pStr) > 0))
	{
		outFmtStr = pStr;
		if (strcmp(outFmtStr, "TTPoints") == 0) {
			pStr = getCmdOption(argv, argv + argc, "-out-json");
			if (pStr && (strlen(pStr) > 0))
			{
                std::ifstream file(pStr);
                std::string strJson((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
				m1transcode.setOutputFormatCustomPointsJson((char*)strJson.c_str());
			}
		}
	}

	bool foundOutFmt = false;
	outFmt = m1transcode.getFormatFromString(outFmtStr);
    if (outFmt > 1) { // if format int is 0 or -1 (making it invalid)
		foundOutFmt = true;
	}
	else {
		cout << "Please select a valid output format" << std::endl;
		return -1;
	}

	pStr = getCmdOption(argv, argv + argc, "-out-file-chans");
	if (pStr != NULL)
		outFileChans = atoi(pStr);
	else
		outFileChans = 0;
	if (!((outFileChans == 0) || (outFileChans == 1) || (outFileChans == 2)))
	{
		cout << "Please select 0, 1, or 2, zero meaning a single, multichannel output file" << std::endl;
		return -1;
	}
	// if "-extract-metadata arg detected, analyze and extract xml metadata
	if (extractMetadata)
	{
		if (admParse.locateMetadata(infilename).mdStartIndex > 0)
		{
			admParse.exportMetadata(infilename, md_outfilename, admParse.locateMetadata(infilename).mdStartIndex, admParse.locateMetadata(infilename).mdEndIndex, admParse.locateMetadata(infilename).totalFileSize);
		}
	}
	cout << std::endl;

	//=================================================================
	// initialize inputs, outputs and components
	//

	// -- input file ---------------------------------------
	// determine number of input files
	SndfileHandle *infile[Mach1TranscodeMAXCHANS];
	vector<string> fNames;
    split(infilename, ' ', fNames);
    audiofileInfo inputInfo;
    
    if (useAudioTimeline) {
        for (int i = 0; i < audioObjects.size(); i++) {
            std::string filename = std::string(infolder) + "/" + audioObjects[i].getName() + ".wav";
            fNames.push_back(filename);
        }
    } else {
        fNames.push_back(infilename);
    }
    
	size_t numInFiles = fNames.size();
	for (int i = 0; i < numInFiles; i++) {
		infile[i] = new SndfileHandle(fNames[i].c_str());
		if (infile[i] && (infile[i]->error() == 0)) {
			// print input file stats
			cout << "Input File:         " << fNames[i] << std::endl;
            inputInfo = printFileInfo(*infile[i]);
			sampleRate = (long)infile[i]->samplerate();
			//            int inChannels = 0;
			//            for (int i = 0; i < numInFiles; i++)
			//                inChannels += infile[i]->channels();
			//            parseFile(*infile[i], inChannels);
		} else {
			cerr << "Error: opening in-file: " << fNames[i] << std::endl;
			return -1;
		}
	}

	cout << "Master Gain:        " << m1transcode.level2db(masterGain) << "dB" << std::endl;
    cout << std::endl;

	for (int i = 0; i < numInFiles; i++) {
		infile[i]->seek(0, 0); // rewind input
	}

	// -- setup 
	m1transcode.setInputFormat(inFmt);
	m1transcode.setOutputFormat(outFmt);
	m1transcode.setLFESub(subChannelIndices, sampleRate);

	// first init of TT points
	if (useAudioTimeline) {
		audioObjects = m1audioTimeline.getAudioObjects();
		std::vector<Mach1Point3D> points;
		for (int i = 0; i < audioObjects.size(); i++) {
			Mach1KeyPoint keypoint = audioObjects[i].getKeyPoints()[0];
			points.push_back(keypoint.point);
		}
		m1transcode.setInputFormatCustomPoints(points);
	}

	// -- output file(s) --------------------------------------

	channels = m1transcode.getOutputNumChannels();
	SndFileWriter outfiles[Mach1TranscodeMAXCHANS];
	int actualOutFileChannels = outFileChans == 0 ? channels : outFileChans;

	if (actualOutFileChannels == 0) {
		cout << "Output channels count is 0!" << std::endl;
		return -1;
	}

	int numOutFiles = channels / actualOutFileChannels;

	for (int i = 0; i < Mach1TranscodeMAXCHANS; i++) {
		memset(inBuffers[i], 0, sizeof(inBuffers[i]));
		memset(outBuffers[i], 0, sizeof(outBuffers[i]));
	}

	//=================================================================
	//  print intermediate formats path
	//
	if (!m1transcode.processConversionPath()) {
		printf("Can't found conversion between formats!");
		return -1;
	} else {
        std::vector<int> formatsConvertionPath = m1transcode.getFormatConversionPath();
		printf("Conversion Path:    ");
		for (int k = 0; k < formatsConvertionPath.size(); k++) {
            printf("%s", m1transcode.getFormatName(formatsConvertionPath[k]).c_str());
			if (k < formatsConvertionPath.size() - 1) {
				printf(" > ");
			}
		}
		printf("\r\n");
	}

	vector<vector<float>> matrix = m1transcode.getMatrixConversion();

	//=================================================================
	//  main sound loop
	// 

	int inChannels = 0;
	for (int i = 0; i < numInFiles; i++)
		inChannels += infile[i]->channels();
	sf_count_t numBlocks = infile[0]->frames() / BUFFERLEN; // files must be the same length
	totalSamples = 0;
	float peak = 0.0f;

	for (int pass = 1, countPasses = ((normalize || spatialDownmixerMode) ? 2 : 1); pass <= countPasses; pass++)
	{
		if (pass == 2) {
			// Mach1 Spatial Downmixer
			// Triggered due to correlation of top vs bottom
			// being higher than threshold
            if (spatialDownmixerMode && outFmt == m1transcode.getFormatFromString("M1Spatial")) {
                m1transcode.setSpatialDownmixer(corrThreshold);
				if (m1transcode.getSpatialDownmixerPossibility()) {
					/*
					std::vector<float> avgSamplesDiff = spatialDownmixChecker.getAvgSamplesDiff();
					for (int i = 0; i < avgSamplesDiff.size(); i++) {
						printf("Average samples diff: %f\r\n", avgSamplesDiff[i]);
					}
					*/ 

                    // reinitialize inputs and outputs
					outFmt = m1transcode.getFormatFromString("M1Horizon");
					m1transcode.setOutputFormat(outFmt);
					m1transcode.processConversionPath();

					channels = m1transcode.getOutputNumChannels();
					actualOutFileChannels = outFileChans == 0 ? channels : outFileChans;
					numOutFiles = channels / actualOutFileChannels;

                    printf("Spatial Downmix:    ");
                    printf("%s", m1transcode.getFormatName(outFmt).c_str());
                    printf("\r\n");
				}
			}

			// normalize
			if (normalize)
			{
				cout << "Reducing gain by " << m1transcode.level2db(peak) << std::endl;
				masterGain /= peak;
			}

			totalSamples = 0;
			for (int file = 0; file < numInFiles; file++)
				infile[file]->seek(0, SEEK_SET);
		}

		if (pass == countPasses) {
			// init outfiles
			for (int i = 0; i < numOutFiles; i++) {
				//TODO: expand this out to other output types and better handling from printFileInfo()
				int format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
				int inputFormat = infile[0]->format() & 0xffff;
				if (inputFormat == SF_FORMAT_PCM_16) format = SF_FORMAT_WAV | SF_FORMAT_PCM_16;
				if (inputFormat == SF_FORMAT_PCM_24) format = SF_FORMAT_WAV | SF_FORMAT_PCM_24;
				if (inputFormat == SF_FORMAT_FLOAT)  format = SF_FORMAT_WAV | SF_FORMAT_FLOAT;
				char outfilestr[1024];
				if (numOutFiles > 1) {
					sprintf(outfilestr, "%s_%0d.wav", outfilename, i);
				}
				else {
					strcpy(outfilestr, outfilename);
				}

				if (outFmt == m1transcode.getFormatFromString("DolbyAtmosSevenOneTwo")) {
					std::string axmlChunkAdmCorrectedString = getTimecode(axmlChunkAdmString, inputInfo.duration).c_str();
					bw64::AxmlChunk axmlChunkAdmCorrected(axmlChunkAdmCorrectedString);
					outfiles[i].open(outfilestr, actualOutFileChannels, chnaChunkAdm, axmlChunkAdmCorrected);
				}
				else {
					outfiles[i].open(outfilestr, (int)sampleRate, actualOutFileChannels, format);
				}

				if (outfiles[i].isOpened()) {
					// set clipping mode
					outfiles[i].setClip();
					// output file stats
					cout << "Output File:        " << outfilestr << std::endl;
					outfiles[i].printInfo();
				}
				else {
					cerr << "Error: opening out-file: " << outfilestr << std::endl;
					return -1;
				}
				if (outFmt == m1transcode.getFormatFromString("M1Spatial")) {
					outfiles[i].setString(0x05, "mach1spatial-8");
				}
				else if (outFmt == m1transcode.getFormatFromString("M1Horizon")) {
					outfiles[i].setString(0x05, "mach1horizon-4");
				}
				else if (outFmt == m1transcode.getFormatFromString("M1HorizonPairs")) {
					outfiles[i].setString(0x05, "mach1horizon-8");
				}
			}
			cout << std::endl;
		}

		// get start samples for all objects (ADM format)
		std::vector<long long> startSampleForAudioObject;
		for (int i = 0; i < audioObjects.size(); i++) {
			startSampleForAudioObject.push_back(audioObjects[i].getKeyPoints()[0].sample);
		}

		for (int i = 0; i <= numBlocks; i++) {
			// read next buffer from each infile
			sf_count_t samplesRead = 0;
			sf_count_t firstBuf = 0;
			float(*inBuf)[Mach1TranscodeMAXCHANS][BUFFERLEN] = (float(*)[Mach1TranscodeMAXCHANS][BUFFERLEN])&(inBuffers[0][0]);
			for (int file = 0; file < numInFiles; file++) {
				sf_count_t thisChannels = infile[file]->channels();

				// first fill buffer with zeros
				for (int j = 0; j < BUFFERLEN; j++)
					for (int k = 0; k < thisChannels; k++) {
						(*inBuf)[firstBuf + k][j] = 0;
					}

				int startSample = 0;
				if (useAudioTimeline) {
					startSample = startSampleForAudioObject[file];
				}
				if (totalSamples + BUFFERLEN >= startSample) {
					sf_count_t framesToRead = thisChannels * BUFFERLEN;

					// cut samples if the beginning of the offset does not match with the beginning of the buffer
					sf_count_t offset = 0;
					if (startSample + BUFFERLEN < totalSamples && totalSamples < startSample) {
						offset = startSample - totalSamples;
						framesToRead = BUFFERLEN + totalSamples - startSample;
					}

					sf_count_t framesReaded = infile[file]->read(fileBuffer, framesToRead);
					samplesRead = framesReaded / thisChannels;
					// demultiplex into process buffers
					float *ptrFileBuffer = fileBuffer;
					for (int j = 0; j < samplesRead; j++) {
						for (int k = 0; k < thisChannels; k++) {
							(*inBuf)[firstBuf + k][offset + j] = *ptrFileBuffer++;
						}
					}
				}

				firstBuf += thisChannels;
			}
			totalSamples += samplesRead;

			m1transcode.processConversion(inPtrs, outPtrs, (int)samplesRead);

			if (pass == 1) {
				if (normalize) {
					// find max
					peak = (std::max)(peak, m1transcode.processNormalization(outPtrs, samplesRead));
				}
			}

			if (pass == countPasses) {
				m1transcode.processMasterGain(outPtrs, samplesRead, masterGain);

				// multiplex to output channels with master gain
				float *ptrFileBuffer = fileBuffer;
				float(*outBuf)[Mach1TranscodeMAXCHANS][BUFFERLEN] = (float(*)[Mach1TranscodeMAXCHANS][BUFFERLEN])&(outBuffers[0][0]);

				for (int file = 0; file < numOutFiles; file++) {
                    for (int j = 0; j < samplesRead; j++) {
                        for (int k = 0; k < actualOutFileChannels; k++) {
							*ptrFileBuffer++ = (*outBuf)[(file*actualOutFileChannels) + k][j];
                        }
                    }
                }

				// write to outfile
				for (int j = 0; j < numOutFiles; j++) {
					outfiles[j].write(fileBuffer + (j*actualOutFileChannels*samplesRead), samplesRead);
				}
			}
		}
	}
	// print time played
	cout << "Length (sec):       " << (float)totalSamples / (float)sampleRate << std::endl;
	return 0;
}
