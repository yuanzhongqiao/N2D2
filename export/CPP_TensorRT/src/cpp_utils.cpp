/*
    (C) Copyright 2017 CEA LIST. All Rights Reserved.
    Contributor(s): Olivier BICHLER (olivier.bichler@cea.fr)
                    David BRIAND (david.briand@cea.fr)

    This software is governed by the CeCILL-C license under French law and
    abiding by the rules of distribution of free software.  You can  use,
    modify and/ or redistribute the software under the terms of the CeCILL-C
    license as circulated by CEA, CNRS and INRIA at the following URL
    "http://www.cecill.info".

    As a counterpart to the access to the source code and  rights to copy,
    modify and redistribute granted by the license, users are provided only
    with a limited warranty  and the software's author,  the holder of the
    economic rights,  and the successive licensors  have only  limited
    liability.

    The fact that you are presently reading this means that you have had
    knowledge of the CeCILL-C license and that you accept its terms.
*/

#include "cpp_utils.hpp"

void getFilesList(const std::string dir,
                                           std::vector<std::string>& files)
{
    struct dirent* pFile;
    DIR* pDir = opendir(dir.c_str());
    if (pDir == NULL)
        throw std::runtime_error(
            "Couldn't open the directory for input patterns: " + dir);

    while ((pFile = readdir(pDir)) != NULL) {
        if (pFile->d_name[0] != '.')
            files.push_back(std::string(dir + "/" + pFile->d_name));
    }
    closedir(pDir);
    std::sort(files.begin(), files.end());
}

void envRead(const std::string& fileName,
                                      unsigned int size,
                                      unsigned int channelsHeight,
                                      unsigned int channelsWidth,
                                      float* data,
                                      bool noLabels,
                                      unsigned int outputsSize,
                                      int32_t* outputTargets)
{
    std::ifstream stimuli(fileName.c_str(), std::fstream::binary);

    if (!stimuli.good())
        throw std::runtime_error("Could not open file: " + fileName);

    char header[2];
    stimuli.read(reinterpret_cast<char*>(&header[0]), sizeof(header));

    if (header[0] != 'P' || (header[1] != '5' && header[1] != '6'))
        throw std::runtime_error("Unknown PGM file format for file: "
                                 + fileName);

    int pixelWidth;
    int pixelHeight;
    int maxValue;

    if (!(stimuli >> pixelWidth) || !(stimuli >> pixelHeight)
        || !(stimuli >> maxValue))
        throw std::runtime_error("Error reading PGM image file: " + fileName);

    stimuli.get();

    if (pixelWidth != (int)channelsWidth || pixelHeight != (int)channelsHeight)
        throw std::runtime_error(
            "PGM image size does not match array size for file: " + fileName);

#if NB_BITS > 0 && NB_BITS != 8 && NB_BITS != 16 && NB_BITS != 32 && NB_BITS   \
                                                                     != 64
#if NB_BITS > 0 && NB_BITS < 8
    char inputsFixed[size];
#elif NB_BITS > 8 && NB_BITS < 16
    short inputsFixed[size];
#elif NB_BITS > 16 && NB_BITS < 32
    int inputsFixed[size];
#elif NB_BITS > 32 && NB_BITS < 64
    long long int inputsFixed[size];
#endif
    stimuli.read(reinterpret_cast<char*>(&inputsFixed[0]),
                 size * sizeof(inputsFixed[0]));

    for (unsigned int i = 0; i < size; ++i)
        data[i] = (float)inputsFixed[i];
#else
    stimuli.read(reinterpret_cast<char*>(&data[0]), size * sizeof(data[0]));
#endif
    if(!noLabels)
        stimuli.read(reinterpret_cast<char*>(&outputTargets[0]),
                    outputsSize * sizeof(outputTargets[0]));

    if (stimuli.eof())
        throw std::runtime_error(
            "End-of-file reached prematurely in data file: " + fileName);
    else if (!stimuli.good())
        throw std::runtime_error("Error while reading data file: " + fileName);
    else if (stimuli.get() != std::fstream::traits_type::eof())
        //throw std::runtime_error("Data file size larger than expected: "
        //                         + fileName);
        std::cout << "Data file size larger than expected: " << fileName << std::endl;

}

/**** Confusion Matrix ****/
void confusion_print(unsigned int nbOutputs,
                                              unsigned int* confusion)
{
    std::cout << "\nConfusion matrix:\n";
    std::cout << std::string(9 + 10 * nbOutputs, '-') << "\n";
    std::cout << "| T \\ E |";

    for (unsigned int estimated = 0; estimated < nbOutputs; ++estimated)
        std::cout << " " << std::setfill(' ') << std::setw(7) << estimated
                  << " |";

    std::cout << "\n" << std::string(9 + 10 * nbOutputs, '-') << "\n";

    unsigned int total = 0;
    unsigned int totalCorrect = 0;

    for (unsigned int target = 0; target < nbOutputs; ++target) {
        unsigned int targetCount = 0;

        for (unsigned int estimated = 0; estimated < nbOutputs; ++estimated)
            targetCount += confusion[estimated + target * nbOutputs];

        total += targetCount;
        totalCorrect += confusion[target + target * nbOutputs];

        std::cout << "| " << std::setfill(' ') << std::setw(5) << target
                  << " |";

        for (unsigned int estimated = 0; estimated < nbOutputs; ++estimated)
            std::cout << " " << std::setfill(' ') << std::setw(7)
                      << confusion[estimated + target * nbOutputs] << " |";

        std::cout << "\n";
        std::cout << "|       |";

        for (unsigned int estimated = 0; estimated < nbOutputs; ++estimated) {
            std::cout << " " << ESC_BG_LIGHT_YELLOW << std::setfill(' ')
                      << std::setw(6) << std::fixed << std::setprecision(2)
                      << 100.0
                         * ((targetCount > 0)
                                ? (confusion[estimated + target * nbOutputs]
                                   / (double)targetCount)
                                : 0.0) << "%" << ESC_ALL_OFF << " |";
        }
        std::cout << "\n";
    }

    std::cout << std::string(9 + 10 * nbOutputs, '-') << "\n"
              << "T: Target    E: Estimated" << std::endl;
}

#ifdef CV_SUPPORT


cv::Mat cv_read(const std::string& fileName)
{
    cv::Mat data = cv::imread(fileName, CV_LOAD_IMAGE_UNCHANGED);

    if (!data.data)
        throw std::runtime_error("cpp_utils::read_cv(): unable to read image: "
                                 + fileName);

    return data;
}

void cv_ColorSpaceTransformation(cv::Mat& frame, CV_ColorSpace colorspace)
{
    cv::Mat frameCvt;

    if (colorspace == BGR) {
        if (frame.channels() == 1) {
            cv::cvtColor(frame, frameCvt, CV_GRAY2BGR);
            frame = frameCvt;
        } else if (frame.channels() == 4) {
            cv::cvtColor(frame, frameCvt, CV_BGRA2BGR);
            frame = frameCvt;
        }
    }
    else if (colorspace == RGB) {
        if (frame.channels() == 1) {
            cv::cvtColor(frame, frameCvt, CV_GRAY2RGB);
            frame = frameCvt;
        }
        else if (frame.channels() == 3) {
            cv::cvtColor(frame, frameCvt, CV_BGR2RGB);
            frame = frameCvt;
        }
        else if (frame.channels() == 4) {
            cv::cvtColor(frame, frameCvt, CV_RGBA2BGR);
            frame = frameCvt;
        }

    }
    else if (frame.channels() > 1) {
        if (colorspace == HSV)
            cv::cvtColor(frame, frameCvt, CV_BGR2HSV);
        else if (colorspace == HLS)
            cv::cvtColor(frame, frameCvt, CV_BGR2HLS);
        else if (colorspace == YCrCb)
            cv::cvtColor(frame, frameCvt, CV_BGR2YCrCb);
        else if (colorspace == CIELab)
            cv::cvtColor(frame, frameCvt, CV_BGR2Lab);
        else if (colorspace == CIELuv)
            cv::cvtColor(frame, frameCvt, CV_BGR2Luv);

        frame = frameCvt;
    }


}

void cv_DrawBBOX(cv::Mat& frame, float* ROIs, unsigned int nbBBox, unsigned int stepIdx)
{ 
        for(unsigned int i = 0; i < nbBBox; ++i)
    {
        float x = ROIs[0 + i*stepIdx];
        float y = ROIs[1 + i*stepIdx];
        float w = ROIs[2 + i*stepIdx];
        float h = ROIs[3 + i*stepIdx];
        float cls = 0;

        if(stepIdx > 4)
            cls = ROIs[4 + i*stepIdx];

        const cv::Scalar color = cv::Scalar(255 *  (1 / cls),
                                            255 - 255*(1 / cls),
                                            255);

        if(w > 0.0 && h > 0.0)
                cv::rectangle(frame, cv::Rect(x,y,w,h), color, 3);
    }

}

void cv_Display(cv::Mat frame, unsigned int displayW, unsigned int displayH)
{
    const std::string frameWindow = "EXPORT LIVE";

    cv::namedWindow(frameWindow.c_str(), CV_WINDOW_NORMAL);
    cvMoveWindow(frameWindow.c_str(), 0, 0);
    cvResizeWindow(frameWindow.c_str(), displayW, displayH);

    if (frame.data)
        cv::imshow(frameWindow.c_str(), frame);
}

void cv_toBuffer(cv::Mat& frame, float * &input)
{

    for(unsigned int ch = 0; ch < frame.channels(); ++ch)
        for (int y = 0; y < frame.rows; ++y)
            for (int x = 0; x < frame.cols; ++x)
                input[x + y*frame.cols + ch*frame.cols*frame.rows] 
                    = frame.at<cv::Vec3b>(y,x)[ch]; 


}



#endif