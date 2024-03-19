#include <gtest/gtest.h>
#include <raft-kv/common/random_device.h>
#include <iostream>
using namespace kv;

TEST(raft, random_device_test) {
    RandomDevice rand(1, 10);
    std::cout<<rand.gen()<<std::endl;
    ASSERT_TRUE(rand.gen() <= 10);
}