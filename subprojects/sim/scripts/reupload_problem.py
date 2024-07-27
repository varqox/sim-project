#!/usr/bin/env python3

from requests import Request, Session
import time
import getpass
import argparse

def parse_api_response(data):
	if not isinstance(data, list) or len(data) < 1:
		return data

	def transform(names, data):
		def to_obj(names, arr):
			obj = {}
			for name_elem, arr_elem in zip(names, arr):
				if isinstance(name_elem, dict) and 'name' in name_elem:
					obj[name_elem['name']] = transform(name_elem, arr_elem)
				else:
					obj[name_elem] = arr_elem
			return obj

		if 'columns' in names:
			res = [to_obj(names['columns'], elem) for elem in data]
		else:
			res = to_obj(names['fields'], data)
		return res

	return transform(data[0], data[1:])


def get_job_info(ses, host, job_id):
	req = Request('POST', '{}/api/jobs/={}'.format(host, job_id), data = {
			'csrf_token': ses.cookies.get_dict()['csrf_token'],
		})
	resp = ses.send(ses.prepare_request(req))
	return parse_api_response(resp.json())

def add_or_reupload_problem(action):
	assert action in ('add', 'reupload')
	parser = argparse.ArgumentParser(description='Process some integers.')
	parser.add_argument('host', help='address of the Sim server')
	parser.add_argument('user', help='user to use (default: sim)', nargs='?', default='sim')
	if action == 'reupload':
		parser.add_argument('problem_id', help='id of the problem to reupload', type=int)
	parser.add_argument('package_path', help='path to zipped problem package')
	parser.add_argument('-p', '--password', help='user password')
	parser.add_argument('-a', '--ask-for-password', help='ask for the user password', action='store_true')
	parser.add_argument('-n', '--name', help='override problem name', default='')
	parser.add_argument('-l', '--label', help='override problem label', default='')
	parser.add_argument('-m', '--memory', help='override memory limit', default='')
	parser.add_argument('-g', '--global-time-limit', help='override every time limit', default='')
	parser.add_argument('-t', '--type', help="problem's type (default: private)", choices=['public','contest-only', 'private'], default='private')
	parser.add_argument('-r', '--reset-time-limits', help='reset time limits using main solution', action='store_true')
	parser.add_argument('-s', '--seek-for-new-tests', help='seek for new tests', action='store_true')
	parser.add_argument('-rs', '--reset-scoring', help='reset scoring', action='store_true')
	parser.add_argument('-i', '--ignore-simfile', help='ignore simfile', action='store_true')
	parser.add_argument('--no-wait', help="don't wait for the scheduled job to finish", action='store_true')
	args = parser.parse_args()

	host = args.host
	user = args.user
	password = 'sim' if args.user == 'sim' else ''
	if args.password:
		password = args.password
	if action == 'reupload':
		problem_id = args.problem_id
	package_path = args.package_path

	if args.ask_for_password:
		print("Type password to the account: ", user)
		password = getpass.getpass()

	# Log in
	ses = Session()
	req = Request('POST', host + "/login", data = {"username" : user, "password" : password})
	resp = ses.send(ses.prepare_request(req))
	resp.raise_for_status()
	if 'Invalid username or password' in resp.text:
		raise RuntimeError("Failed to log in")

	# Add or reupload problem
	if action == 'reupload':
		url = '{}/api/problem/{}/reupload'.format(host, problem_id)
	elif action == 'add':
		url = '{}/api/problem/add'.format(host)
	else:
		assert False
	req = Request(
		'POST',
		url,
		data={
			'name': args.name,
			'label': args.label,
			'type': {'public': 'PUB', 'contest-only': 'CON', 'private': 'PRI'}[args.type],
			'mem_limit': args.memory,
			'global_time_limit': args.global_time_limit,
			'csrf_token': ses.cookies.get_dict()['csrf_token'],
			**{k: v for k, v in {
				'reset_time_limits': args.reset_time_limits,
				'seek_for_new_tests': args.seek_for_new_tests,
				'reset_scoring': args.reset_scoring,
				'ignore_simfile': args.ignore_simfile,
			}.items() if v}
		},
		files=[('package', ('package.zip', open(package_path, 'rb'), 'application/zip'))]
	)
	resp = ses.send(ses.prepare_request(req))
	resp.raise_for_status()
	job_id = resp.text
	print('Scheduled a job for the problem {}.'.format(action))
	if args.no_wait:
		return

	print('Waiting for the job (id {}) to finish...'.format(job_id))
	while True:
		job_info = get_job_info(ses, host, job_id)[0]
		status = job_info['status']['text']
		if status in ('Done', 'Failed', 'Cancelled'):
			break
		time.sleep(0.1)

	print('Job finished with status: {}'.format(status))
	if status != 'Done':
		raise RuntimeError('Job finished with status other than Done: {}'.format(status));

if __name__ == '__main__':
	add_or_reupload_problem('reupload')
