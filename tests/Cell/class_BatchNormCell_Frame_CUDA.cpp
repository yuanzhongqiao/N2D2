/*
    (C) Copyright 2016 CEA LIST. All Rights Reserved.
    Contributor(s): Olivier BICHLER (olivier.bichler@cea.fr)

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

#ifdef CUDA
#include <cudnn.h>
#if CUDNN_VERSION >= 4000

#include "N2D2.hpp"

#include "Cell/BatchNormCell_Frame_CUDA.hpp"
#include "Cell/ConvCell_Frame_CUDA.hpp"
#include "DeepNet.hpp"
#include "Xnet/Environment.hpp"
#include "Xnet/Network.hpp"
#include "utils/UnitTest.hpp"

using namespace N2D2;

template <class T>
class BatchNormCell_Frame_CUDA_Test : public BatchNormCell_Frame_CUDA<T> {
public:
    BatchNormCell_Frame_CUDA_Test(const DeepNet& deepNet, 
                                  const std::string& name,
                                  unsigned int nbOutputs,
                                  const std::shared_ptr
                                  <Activation>& activation)
        : Cell(deepNet, name, nbOutputs),
          BatchNormCell(deepNet, name, nbOutputs),
          BatchNormCell_Frame_CUDA<T>(deepNet, name, nbOutputs, activation) {};

    friend class UnitTest_BatchNormCell_Frame_CUDA_float_setScales;
    friend class UnitTest_BatchNormCell_Frame_CUDA_float_addInput__env;
    friend class UnitTest_BatchNormCell_Frame_CUDA_float_addInput;
    friend class UnitTest_BatchNormCell_Frame_CUDA_double_setScales;
    friend class UnitTest_BatchNormCell_Frame_CUDA_double_addInput__env;
    friend class UnitTest_BatchNormCell_Frame_CUDA_double_addInput;
    friend class UnitTest_BatchNormCell_Frame_CUDA_half_setScales;
    friend class UnitTest_BatchNormCell_Frame_CUDA_half_addInput__env;
    friend class UnitTest_BatchNormCell_Frame_CUDA_half_addInput;
};

////////////////////////////////////////////////////////////////////////////////
// float
////////////////////////////////////////////////////////////////////////////////
TEST(BatchNormCell_Frame_CUDA_float, setScales)
{
    REQUIRED(UnitTest::CudaDeviceExists(3));

    Network net(0U,false);
    DeepNet dn(net);
    Environment env(net, EmptyDatabase, {10, 10, 1});

    BatchNormCell_Frame_CUDA_Test<float> bn1(
        dn, "bn1", 1, std::shared_ptr<Activation>());
    BatchNormCell_Frame_CUDA_Test<float> bn2(
        dn, "bn2", 1, std::shared_ptr<Activation>());

    bn1.addInput(env);
    bn2.addInput(env);

    bn2.setScales(bn1.getScales());
    bn1.initialize();
    bn2.initialize();

    Tensor<float> bn1Scale;
    Tensor<float> bn2Scale;
    bn1.getScale(0, bn1Scale);
    bn2.getScale(0, bn2Scale);

    ASSERT_EQUALS(bn1Scale(0), 1.0);
    ASSERT_EQUALS(bn2Scale(0), 1.0);

    Tensor<float> bn1ScaleSet({1}, 2.0);
    bn1.setScale(0, bn1ScaleSet);

    bn1Scale.clear();
    bn2Scale.clear();
    bn1.getScale(0, bn1Scale);
    bn2.getScale(0, bn2Scale);

    ASSERT_EQUALS(bn1Scale(0), 2.0);
    ASSERT_EQUALS(bn2Scale(0), 2.0);
}

TEST_DATASET(BatchNormCell_Frame_CUDA_float,
             addInput__env,
             (unsigned int channelsWidth, unsigned int channelsHeight),
             std::make_tuple(24U, 24U),
             std::make_tuple(24U, 32U),
             std::make_tuple(32U, 24U))
{
    REQUIRED(UnitTest::CudaDeviceExists(3));

    Network net(0U,false);
    DeepNet dn(net);
    Environment env(net, EmptyDatabase, {channelsWidth, channelsHeight, 1});

    BatchNormCell_Frame_CUDA_Test<float> bn1(
        dn, "bn1", 1, std::shared_ptr<Activation>());
    bn1.addInput(env);
    bn1.initialize();

    ASSERT_EQUALS(bn1.getNbChannels(), 1U);
    ASSERT_EQUALS(bn1.getChannelsWidth(), channelsWidth);
    ASSERT_EQUALS(bn1.getChannelsHeight(), channelsHeight);
    ASSERT_EQUALS(bn1.getNbOutputs(), 1U);
    ASSERT_EQUALS(bn1.getOutputsWidth(), channelsWidth);
    ASSERT_EQUALS(bn1.getOutputsHeight(), channelsHeight);
    // BatchNormCell_Frame_CUDA::backPropagate() doesn't work with empty
    // mDiffOutputs
    // ASSERT_NOTHROW_ANY(bn1.checkGradient(1.0e-3, 1.0e-2));

    // Internal state testing
    ASSERT_EQUALS(bn1.mInputs.dataSize(), channelsWidth * channelsHeight);
    ASSERT_EQUALS(bn1.mOutputs.size(), channelsWidth * channelsHeight);
    ASSERT_EQUALS(bn1.mDiffInputs.size(), channelsWidth * channelsHeight);
    ASSERT_EQUALS(bn1.mDiffOutputs.dataSize(), 0U);
}

TEST_DATASET(BatchNormCell_Frame_CUDA_float,
             addInput,
             (unsigned int channelsWidth, unsigned int channelsHeight),
             std::make_tuple(24U, 24U),
             std::make_tuple(24U, 32U),
             std::make_tuple(32U, 24U))
{
    REQUIRED(UnitTest::CudaDeviceExists(3));

    const unsigned int nbOutputs = 16;

    Network net(0U,false);
    DeepNet dn(net);
    Environment env(net, EmptyDatabase, {channelsWidth, channelsHeight, 1});

    ConvCell_Frame_CUDA<float> conv1(dn, "conv1",
                              std::vector<unsigned int>({3, 3}),
                              nbOutputs,
                              std::vector<unsigned int>({1, 1}),
                              std::vector<unsigned int>({1, 1}),
                              std::vector<int>({0, 0}),
                              std::vector<unsigned int>({1U, 1U}),
                              std::make_shared
                              <TanhActivation_Frame_CUDA<float> >());
    BatchNormCell_Frame_CUDA_Test<float> bn1(
        dn, "bn1", nbOutputs, std::shared_ptr<Activation>());

    conv1.addInput(env);
    bn1.addInput(&conv1);
    conv1.initialize();
    bn1.initialize();

    ASSERT_EQUALS(bn1.getNbChannels(), nbOutputs);
    ASSERT_EQUALS(bn1.getChannelsWidth(), conv1.getOutputsWidth());
    ASSERT_EQUALS(bn1.getChannelsHeight(), conv1.getOutputsHeight());
    ASSERT_EQUALS(bn1.getNbOutputs(), nbOutputs);
    ASSERT_EQUALS(bn1.getOutputsWidth(), conv1.getOutputsWidth());
    ASSERT_EQUALS(bn1.getOutputsHeight(), conv1.getOutputsHeight());
    // ASSERT_NOTHROW_ANY(bn1.checkGradient(1.0e-3, 1.0e-2));

    // Internal state testing
    ASSERT_EQUALS(bn1.mInputs.dataSize(),
                  nbOutputs * conv1.getOutputsWidth()
                  * conv1.getOutputsHeight());
    ASSERT_EQUALS(bn1.mOutputs.size(),
                  nbOutputs * conv1.getOutputsWidth()
                  * conv1.getOutputsHeight());
    ASSERT_EQUALS(bn1.mDiffInputs.size(),
                  nbOutputs * conv1.getOutputsWidth()
                  * conv1.getOutputsHeight());
    ASSERT_EQUALS(bn1.mDiffOutputs.dataSize(),
                  nbOutputs * conv1.getOutputsWidth()
                  * conv1.getOutputsHeight());
}

RUN_TESTS()

#else

#include <iostream>

int main()
{
    std::cout << "BatchNormCell_Frame_CUDA requires CuDNN 4.0" << std::endl;
    return 0;
}

#endif

#else

int main()
{
    return 0;
}

#endif
