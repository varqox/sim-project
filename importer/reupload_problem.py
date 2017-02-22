#!/usr/bin/python3
# -*- coding: utf-8 -*-

from __future__ import print_function
from requests import Request, Session
from bs4 import BeautifulSoup
from enum import Enum
import time
import sys

def eprint(*args, **kwargs):
    print(*args, file=sys.stderr, **kwargs)

global ses
ses = Session()

host = 'http://127.7.7.7:8080'
username = 'sim'
password = 'sim'

def log_html(content):
	f = open('/tmp/lol.html', 'w')
	f.truncate()
	f.write(content)
	f.close()

def login():
	# ses.get(host + '/login')
	req = Request('POST', host + "/login", data = {"username" : username, "password" : password})
	ses.send(ses.prepare_request(req))

def ReuploadProblem(path, old_pid):
	csrf_token = ses.cookies.get_dict()['csrf_token']
	req = Request('POST', host + "/p/" + old_pid + "/reupload", data = {
			"name" : '',
			"label" : '',
			"memory-limit" : '',
			"global-time-limit" : '',
			"force-auto-limit" : 'on',
			"csrf_token" : csrf_token
		},
		files=[('package', ('xd.zip', open(path, 'rb'), 'application/zip'))])
	resp = ses.send(ses.prepare_request(req))
	log_html(resp.text)

	assert resp.status_code == 200
	return resp.url[resp.url.rfind('/') + 1:] # return the job id

class JobStatus(Enum):
	DONE = 1
	FAILED = 2
	CANCELLED = 3

def waitJob(job_id : str):
	while True:
		resp = ses.get(host + "/jobs/" + job_id)
		bs = BeautifulSoup(resp.text, "lxml")
		status = bs.find('td', class_ = 'status').text

		if status == "Done":
			return JobStatus.DONE
		elif status == "Failed":
			return JobStatus.FAILED
		elif status == "Cancelled":
			return JobStatus.CANCELLED
		# Wait a moment
		time.sleep(.3)

def getproblemId(job_id : str):
	resp = ses.get(host + "/jobs/" + job_id)
	bs = BeautifulSoup(resp.text, "lxml")
	buttons = bs.find_all('a', class_ = 'btn-small')
	for item in buttons:
		if item.text == "View problem":
			url = item['href']
			return url[url.rfind('/') + 1:] # Return the problem's id


##############################################
if len(sys.argv) < 3:
	eprint("You have to specify the problem's path as the first argument and problem id to reupload")
	sys.exit(1)

problem_path = sys.argv[1]
old_pid = sys.argv[2]
eprint("problem_path:", problem_path)

login() # Begin session

job_id = ReuploadProblem(problem_path, old_pid)
# job_id = '163'
# eprint(job_id)

assert waitJob(job_id) == JobStatus.DONE
eprint("Done.")
