#!/usr/bin/python3
# -*- coding: utf-8 -*-

from __future__ import print_function
from requests import Request, Session
from bs4 import BeautifulSoup
from enum import Enum
import time
import sys
import getpass
import json

def eprint(*args, **kwargs):
	print(*args, file=sys.stderr, **kwargs)

global ses
ses = Session()

host = 'http://127.7.7.7:8080'
username = 'sim'
password = 'sim'

if "-p" in sys.argv:
	print("Give password to the account:", username)
	password = getpass.getpass()

def log_html(content):
	f = open('/tmp/lol.html', 'w')
	f.truncate()
	f.write(content)
	f.close()

def login():
	# ses.get(host + '/login')
	req = Request('POST', host + "/login", data = {"username" : username, "password" : password})
	ses.send(ses.prepare_request(req))

def AddProblemToProblemset(path):
	csrf_token = ses.cookies.get_dict()['csrf_token']
	req = Request('POST', host + "/api/problem/add", data = {
			"name" : '',
			"label" : '',
			"mem_limit" : '',
			"global_time_limit" : '',
			# "force-auto-limit" : 'on',
			"type" : 'PRI',
			"csrf_token" : csrf_token
		},
		files=[('package', ('xd.zip', open(path, 'rb'), 'application/zip'))])
	resp = ses.send(ses.prepare_request(req))
	log_html(resp.text)

	assert resp.status_code == 200
	return resp.text # return the job id

class JobStatus(Enum):
	DONE = 1
	FAILED = 2
	CANCELLED = 3

def waitJob(job_id : str):
	while True:
		resp = ses.get(host + "/api/jobs/=" + job_id)
		status = json.loads(resp.text)[0][3][1];

		if status == "Done":
			return JobStatus.DONE
		elif status == "Failed":
			return JobStatus.FAILED
		elif status == "Cancelled":
			return JobStatus.CANCELLED
		# Wait a moment
		time.sleep(.3)

def getproblemId(job_id : str):
	resp = ses.get(host + "/api/jobs/=" + job_id)
	return json.loads(resp.text)[0][7]['problem'];


##############################################
args = [x for x in sys.argv if x[0] != '-']
if len(args) < 2:
	eprint("You have to specify the problem's path as the first non-option argument")
	sys.exit(1)

problem_path = args[1]
eprint("problem_path:", problem_path)

login() # Begin session

job_id = AddProblemToProblemset(problem_path)
# job_id = '163'
# eprint(job_id)

assert waitJob(job_id) == JobStatus.DONE
problem_id = getproblemId(job_id)
eprint("problem_id:", problem_id)
print(problem_id)
