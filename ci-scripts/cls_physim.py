# * Licensed to the OpenAirInterface (OAI) Software Alliance under one or more
# * contributor license agreements.  See the NOTICE file distributed with
# * this work for additional information regarding copyright ownership.
# * The OpenAirInterface Software Alliance licenses this file to You under
# * the OAI Public License, Version 1.1  (the "License"); you may not use this file
# * except in compliance with the License.
# * You may obtain a copy of the License at
# *
# *      http://www.openairinterface.org/?page_id=698
# *
# * Unless required by applicable law or agreed to in writing, software
# * distributed under the License is distributed on an "AS IS" BASIS,
# * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# * See the License for the specific language governing permissions and
# * limitations under the License.
# *-------------------------------------------------------------------------------
# * For more information about the OpenAirInterface (OAI) Software Alliance:
# *      contact@openairinterface.org
# */
#---------------------------------------------------------------------
# Python for CI of OAI-eNB + COTS-UE
#
#   Required Python Version
#     Python 3.x
#
#   Required Python Package
#     pexpect
#---------------------------------------------------------------------

#to use logging.info()
import logging
#to create a SSH object locally in the methods
import sshconnection
#to update the HTML object
import cls_oai_html
import cls_cmd
#for log folder maintenance
import os
import re

#-----------------
# Helper functions
#-----------------
LOG_PATH = 'phy_sim_logs'

def CheckResults_LDPCcudaTest(HTML, runargs, runLogFile):
	#parse results looking for Encoding and Decoding mean values
	runResults=[]
	with open(runLogFile) as f:
		for line in f:
			if 'mean' in line:
				runResults.append(line)
	#the values are appended for each mean value (2), so we take these 2 values from the list
	info = runResults[0] + runResults[1]
	HTML.CreateHtmlTestRowQueue(runargs, 'OK', [info])
	return True

def CheckResults_LDPCt2Test(HTML, runLogFile, runsim, runargs, threshold):
	success = False
	msg = ""
	time = None
	# Determine the search pattern based on the simulation type
	search_pattern = None
	if runsim == 'nr_ulsim':
		search_pattern = r'ULSCH total decoding time\s+(\d+\.\d+)\s+us'
	elif runsim == 'nr_dlsim':
		search_pattern = r'DLSCH encoding time\s+(\d+\.\d+)\s+us'
	else:
		msg = f"Unsupported simulation type '{runsim}'."
		logging.error(msg)
		HTML.CreateHtmlTestRowQueue(runargs, 'KO', [msg])
		return False
	# Parse the log file to check for test success and processing time
	try:
		with open(runLogFile, 'r') as f:
			log_content = f.read()
			# Check if the test was successful
			if 'test OK' not in log_content:
				msg = f'Physim did not succeed, check the log file {runLogFile}.'
				logging.error(msg)
				HTML.CreateHtmlTestRowQueue(runargs, 'KO', [msg])
				return False
			# Search for the processing time
			proc_time_match = re.search(search_pattern, log_content)
			if proc_time_match:
				time = float(proc_time_match.group(1))
				success = time < float(threshold)
				msg = proc_time_match.group(0)
	except Exception as e:
		msg = f'Error while parsing log file {runLogFile}: exception: {e}'
		logging.error(msg)
		HTML.CreateHtmlTestRowQueue(runargs, 'KO', [msg])
		return False
	if success:
		logging.info(msg)
		HTML.CreateHtmlTestRowQueue(runargs, 'OK', [msg])
	else:
		if time is not None:
			msg = f'Processing time {time} us exceeds a limit of {threshold} us'
		else:
			msg = f'Processing time not found in log file {runLogFile}.'
		logging.error(msg)
		HTML.CreateHtmlTestRowQueue(runargs, 'KO', [msg])
	return success

class PhySim:
	def __init__(self):
		self.runargs = ""
		self.timethrs = ""
		self.eNBIpAddr = ""
		self.eNBSourceCodePath = ""

#-----------------$
#PUBLIC Methods$
#-----------------$
	def Run_CUDATest(self, htmlObj, testcase_id):
		workSpacePath = f'{self.eNBSourceCodePath}/cmake_targets'
		#create run logs folder locally
		os.system(f'mkdir -p ./{LOG_PATH}')
		#log file is tc_<testcase_id>.log remotely
		runLogFile=f'physim_{str(testcase_id)}.log'
		#open a session for test run
		with cls_cmd.getConnection(self.eNBIpAddr) as cmd:
			cmd.run(f'{workSpacePath}/ran_build/build/ldpctest {self.runargs} >> {workSpacePath}/{runLogFile}')
			cmd.copyin(src=f'{workSpacePath}/{runLogFile}', tgt=f'{LOG_PATH}/{runLogFile}')
		return CheckResults_LDPCcudaTest(htmlObj, self.runargs, f'{LOG_PATH}/{runLogFile}')

	def Run_T2Test(self, htmlObj, testcase_id):
		workSpacePath = f'{self.eNBSourceCodePath}/cmake_targets'
		#create run logs folder locally
		os.system(f'mkdir -p ./{LOG_PATH}')
		#log file is tc_<testcase_id>.log remotely
		runLogFile=f'physim_{str(testcase_id)}.log'
		#open a session for test run
		with cls_cmd.getConnection(self.eNBIpAddr) as cmd:
			cmd.run(f'sudo {workSpacePath}/ran_build/build/{self.runsim} {self.runargs} >> {workSpacePath}/{runLogFile}')
			cmd.copyin(src=f'{workSpacePath}/{runLogFile}', tgt=f'{LOG_PATH}/{runLogFile}')
		return CheckResults_LDPCt2Test(htmlObj, f'{LOG_PATH}/{runLogFile}', self.runsim, self.runargs, self.timethrs)
