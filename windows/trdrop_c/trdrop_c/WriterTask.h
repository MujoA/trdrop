#pragma once
#ifndef TRDROP_TASKS_POST_WRITER_H
#define TRDROP_TASKS_POST_WRITER_H

#include <functional>
#include <math.h>
#include <sstream>
#include <iomanip>

#include <opencv2\core\core.hpp>
#include <opencv2/highgui/highgui.hpp>  // VideoWriter

#include "Tasks.h"

namespace trdrop {
	namespace tasks {
		namespace post {

			using trdrop::tasks::posttask;

			class WriterTask : public posttask {

				// default member
			public:
				WriterTask() = delete;
				WriterTask(const WriterTask & other) = default;
				WriterTask & operator=(const WriterTask & other) = delete;
				WriterTask(WriterTask && other) = default;
				WriterTask & operator=(WriterTask && other) = delete;
				~WriterTask() = default;

				// specialized member
			public:
				WriterTask(std::string filename, int codec, double bakedFps, cv::Size frameSize)
					: filename(filename)
					, codec(codec)
					, bakedFps(bakedFps)
					, frameSize(frameSize)
				    , output(filename, codec, bakedFps, frameSize)
					, posttask(std::bind(&WriterTask::process
						, this
						, std::placeholders::_1)) {	
					
					// DEBUG
					std::cout << "WriterTask - Output opened: " << (output.isOpened() ? "true" : "false") << '\n';
				}

				// interface methods
			public:
				void process(const cv::Mat & res) {
					output.write(res);
				}
				// private methods
			private:

				// private member
			private:
				cv::VideoWriter output;
				const std::string filename;
				const int codec;
				const double bakedFps;
				const cv::Size frameSize;
			};
		} // namespace post
	} // namespace tasks
} // namespace trdrop

#endif // !TDROP_TASKS_POST_WRITER_H