//*****************************************************************************
// Copyright 2017-2019 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//*****************************************************************************

#pragma once

#include <iostream>

#include "ngraph/distributed.hpp"

#ifdef NGRAPH_DISTRIBUTED_OMPI_ENABLE
#include <mpi.h>

namespace ngraph
{
    namespace distributed
    {
        class OpenMPIDistributedInterface : public DistributedInterface
        {
        public:
            OpenMPIDistributedInterface()
            {
                int flag = 0;
                MPI_Initialized(&flag);
                if (!flag)
                {
                    MPI_Init(NULL, NULL);
                }
            }

            ~OpenMPIDistributedInterface() override
            {
                int flag = 0;
                MPI_Initialized(&flag);

                if (flag)
                {
                    MPI_Finalize();
                }
            }

            int get_size() override
            {
                int size;
                MPI_Comm_size(MPI_COMM_WORLD, &size);
                return size;
            }

            int get_rank() override
            {
                int rank;
                MPI_Comm_rank(MPI_COMM_WORLD, &rank);
                return rank;
            }

            void all_reduce(void* in,
                            void* out,
                            const element::Type& element_type,
                            size_t count) override
            {
                auto data_type = MPI_FLOAT;

                if (element_type == element::f32)
                {
                    data_type = MPI_FLOAT;
                }
                else if (element_type == element::f64)
                {
                    data_type = MPI_DOUBLE;
                }
                else
                {
                    throw std::runtime_error("AllReduce op supports only f32 and f64 types");
                }

                MPI_Allreduce(in, out, count, data_type, MPI_SUM, MPI_COMM_WORLD);
            }

            void broadcast(void* in, const element::Type& element_type, size_t count) override
            {
                auto data_type = MPI_FLOAT;

                if (element_type == element::f64)
                {
                    data_type = MPI_DOUBLE;
                }
                else if (element_type != element::f32)
                {
                    throw std::runtime_error(
                        "BroadcastDistributed op supports only f32 and f64 types");
                }
                MPI_Bcast(in, count, data_type, 0, MPI_COMM_WORLD);
            }
        };
    }
}
#endif