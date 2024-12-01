function assert(condition, optional_message) {
	if (!condition) {
		throw Error('Assertion failed' + optional_message == null ? '' : ': ' + optional_message);
	}
}
function elem_with_text(tag, text) {
	const elem = document.createElement(tag);
	elem.innerText = text;
	return elem;
}
function elem_with_class(tag, classes) {
	const elem = document.createElement(tag);
	elem.className = classes;
	return elem;
}
function elem_with_class_and_text(tag, classes, text) {
	const elem = document.createElement(tag);
	elem.innerText = text;
	elem.className = classes;
	return elem;
}
function elem_with_id(tag, id) {
	const elem = document.createElement(tag);
	elem.id = id;
	return elem;
}
function elem_of(tag, ...args) {
	const elem = document.createElement(tag);
	elem.append(...args);
	return elem;
}
function elem_with_class_of(tag, classes, ...args) {
	const elem = document.createElement(tag);
	elem.className = classes;
	elem.append(...args);
	return elem;
}
function elem_request_status_pending() {
	const elem = elem_with_class('span', 'request-status pending');
	elem.appendChild(elem_with_class('div', 'spinner'));
	return elem;
}
function elem_request_status_success() {
	return elem_with_class('span', 'request-status success');
}
function elem_request_status_error() {
	return elem_with_class('span', 'request-status error');
}
function remove_centered_request_status(parent) {
	parent.querySelector('.request-status').parentNode.remove();
}
function try_remove_centered_request_status(parent) {
	parent.querySelector('.request-status')?.parentNode.remove();
}
// This is very useful when triggering a CSS transition on just appended elements (see: https://stackoverflow.com/a/24195559)
function trigger_reflow_on(elem) {
	elem.offsetWidth;
}
function append_with_fade_in(parent, elem, delay = '0s') {
	elem.classList.add('fade-in');
	parent.appendChild(elem);
	trigger_reflow_on(elem);
	elem.style.transitionDelay = delay;
	elem.style.opacity = 1;
}
function append_with_fade_in_slide_down(parent, elem, delay = '0s') {
	elem.classList.add('fade-in-slide-down');
	parent.appendChild(elem);
	trigger_reflow_on(elem);
	// elem.getBoundingClientRect().height cannot be used as it returns a totally different value here
	elem.style.maxHeight = elem.scrollHeight + 'px'; // No need for adding borders since .fade-in-slide-down has border: none
	elem.style.transitionDelay = delay;
	elem.style.opacity = 1;
	// Adjust max-height on subtree changes
	const mo = new MutationObserver((mutation_records) => {
		for (const record of mutation_records) {
			if (record.target === elem && record.type === 'attributes') {
				continue; // ignore changes to elem's attributes, e.g. the below max-height change
			}
			trigger_reflow_on(elem);
			// elem.getBoundingClientRect().height cannot be used as it returns a totally different value here
			elem.style.maxHeight = elem.scrollHeight + 'px';
			break;
		}
	});
	mo.observe(elem, {subtree: true, childList: true, attributes: true, characterData: true});
}
function remove_children(elem) {
	elem.textContent = '';
}

async function copy_to_clipboard(make_text_to_copy) {
	if (navigator.clipboard) {
		return navigator.clipboard.writeText(make_text_to_copy());
	}

	const elem = document.createElement('textarea');
	elem.style.position = 'absolute';
	elem.style.left = '-9999px';
	elem.value = make_text_to_copy();
	elem.setAttribute('readonly', '');
	document.body.appendChild(elem);
	elem.select();
	document.execCommand('copy');
	elem.remove();
}

/* ============================ URLs ============================ */

function url_api_contest_entry_tokens_add(contest_id) { return `/api/contest/${contest_id}/entry_tokens/add`; }
function url_api_contest_entry_tokens_add_short(contest_id) { return `/api/contest/${contest_id}/entry_tokens/add_short`; }
function url_api_contest_entry_tokens_delete(contest_id) { return `/api/contest/${contest_id}/entry_tokens/delete`; }
function url_api_contest_entry_tokens_delete_short(contest_id) { return `/api/contest/${contest_id}/entry_tokens/delete_short`; }
function url_api_contest_entry_tokens_regenerate(contest_id) { return `/api/contest/${contest_id}/entry_tokens/regenerate`; }
function url_api_contest_entry_tokens_regenerate_short(contest_id) { return `/api/contest/${contest_id}/entry_tokens/regenerate_short`; }
function url_api_contest_entry_tokens_view(contest_id) { return `/api/contest/${contest_id}/entry_tokens`; }
function url_api_contest_name_for_contest_entry_token(contest_entry_token) { return `/api/contest_entry_token/${contest_entry_token}/contest_name`; }
function url_api_job_download_log(job_id) { return `/api/download/job/${job_id}/log`; }
function url_api_jobs() { return '/api/jobs'; }
function url_api_jobs_with_status(status) { return `/api/jobs/status=/${status}`; }
function url_api_problem(problem_id) { return `/api/problem/${problem_id}`; }
function url_api_problem_reupload(problem_id) { return `/api/problem/${problem_id}/reupload`; }
function url_api_problems() { return '/api/problems'; }
function url_api_problems_add() { return '/api/problems/add'; }
function url_api_problems_with_visibility(problem_visibility) { return `/api/problems/visibility=/${problem_visibility}`; }
function url_api_sign_in() { return '/api/sign_in'; }
function url_api_sign_out() { return '/api/sign_out'; }
function url_api_sign_up() { return '/api/sign_up'; }
function url_api_submissions() { return '/api/submissions'; }
function url_api_submissions_with_type(submission_type) { return `/api/submissions/type=/${submission_type}`; }
function url_api_use_contest_entry_token(contest_entry_token) { return `/api/contest_entry_token/${contest_entry_token}/use`; }
function url_api_user(user_id) { return `/api/user/${user_id}`; }
function url_api_user_change_password(user_id) { return `/api/user/${user_id}/change_password`; }
function url_api_user_delete(user_id) { return `/api/user/${user_id}/delete`; }
function url_api_user_edit(user_id) { return `/api/user/${user_id}/edit`; }
function url_api_user_jobs(user_id) { return `/api/jobs/user=/${user_id}`; }
function url_api_user_jobs_with_status(user_id, status) { return `/api/jobs/user=/${user_id}/status=/${status}`; }
function url_api_user_merge_into_another(user_id) { return `/api/user/${user_id}/merge_into_another`; }
function url_api_user_problems(user_id) { return `/api/user/${user_id}/problems`; }
function url_api_user_problems_with_visibility(user_id, problem_visibility) { return `/api/user/${user_id}/problems/visibility=/${problem_visibility}`; }
function url_api_user_submissions(user_id) { return `/api/submissions/user=/${user_id}`; }
function url_api_user_submissions_with_type(user_id, submission_type) { return `/api/submissions/user=/${user_id}/type=/${submission_type}`; }
function url_api_users() { return '/api/users'; }
function url_api_users_add() { return '/api/users/add'; }
function url_api_users_with_type(user_type) { return `/api/users/type=/${user_type}`; }
function url_change_user_password(user_id) { return `/user/${user_id}/change_password`; }
function url_contest(contest_id) { return `/c/c${contest_id}`; }
function url_contest_problem(contest_problem_id) { return `/c/p${contest_problem_id}`; }
function url_contest_round(contest_round_id) { return `/c/r${contest_round_id}`; }
function url_contests() { return '/c'; }
function url_enter_contest(contest_entry_token) { return `/enter_contest/${contest_entry_token}`; }
function url_job(job_id) { return `/jobs/${job_id}`; }
function url_jobs() { return '/jobs'; }
function url_logs() { return '/logs'; }
function url_main_page() { return '/'; }
function url_problem(problem_id) { return `/p/${problem_id}`; }
function url_problem_create_submission(problem_id) { return `/p/${problem_id}/submit`; }
function url_problem_delete(problem_id) { return `/p/${problem_id}/delete`; }
function url_problem_download(problem_id) { return `/api/download/problem/${problem_id}`; }
function url_problem_edit(problem_id) { return `/p/${problem_id}/edit`; }
function url_problem_merge(problem_id) { return `/p/${problem_id}/merge`; }
function url_problem_reset_time_limits(problem_id) { return `/p/${problem_id}/reset_time_limits`; }
function url_problem_reupload(problem_id) { return `/problem/${problem_id}/reupload`; }
function url_problem_solutions(problem_id) { return `/p/${problem_id}#all_submissions#solutions`; }
function url_problem_statement(problem_id, problem_name) { return `/api/download/statement/problem/${problem_id}/${encodeURIComponent(problem_name)}`; }
function url_problems() { return '/problems'; }
function url_problems_add() { return '/problems/add'; }
function url_sign_in() { return '/sign_in'; }
function url_sign_out() { return '/sign_out'; }
function url_sign_up() { return '/sign_up'; }
function url_sim_logo_img() { return '/kit/img/sim-logo.png'; }
function url_submission(submission_id) { return `/s/${submission_id}`; }
function url_submission_source(submission_id) { return `/s/${submission_id}#source`; }
function url_submissions() { return '/submissions'; }
function url_user(user_id) { return `/u/${user_id}`; }
function url_user_delete(user_id) { return `/user/${user_id}/delete`; }
function url_user_edit(user_id) { return `/user/${user_id}/edit`; }
function url_user_merge_into_another(user_id) { return `/user/${user_id}/merge_into_another`; }
function url_users() { return '/users'; }
function url_users_add() { return '/users/add'; }

/* ================================= Humanize ================================= */

/**
 * @brief Converts @p size, so that it human readable
 * @details It adds proper suffixes, for example:
 *   1 -> "1 byte"
 *   1023 -> "1023 bytes"
 *   1024 -> "1.0 KiB"
 *   129747 -> "127 KiB"
 *   97379112 -> "92.9 MiB"
 *
 * @param size size to humanize
 *
 * @return humanized file size
 */
function humanize_file_size(size) {
	const MIN_KIB = 1024;
	const MIN_MIB = 1048576;
	const MIN_GIB = 1073741824;
	const MIN_TIB = 1099511627776;
	const MIN_PIB = 1125899906842624;
	const MIN_EIB = 1152921504606846976;
	const MIN_3DIGIT_KIB = 102349;
	const MIN_3DIGIT_MIB = 104805172;
	const MIN_3DIGIT_GIB = 107320495309;
	const MIN_3DIGIT_TIB = 109896187196212;
	const MIN_3DIGIT_PIB = 112533595688920269;
	// Bytes
	if (size < MIN_KIB) {
		return (size === 1 ? "1 byte" : size + " bytes");
	}
	// KiB
	if (size < MIN_3DIGIT_KIB) {
		return parseFloat(size / MIN_KIB).toFixed(1) + " KiB";
	}
	if (size < MIN_MIB) {
		return Math.round(size / MIN_KIB) + " KiB";
	}
	// MiB
	if (size < MIN_3DIGIT_MIB) {
		return parseFloat(size / MIN_MIB).toFixed(1) + " MiB";
	}
	if (size < MIN_GIB) {
		return Math.round(size / MIN_MIB) + " MiB";
	}
	// GiB
	if (size < MIN_3DIGIT_GIB) {
		return parseFloat(size / MIN_GIB).toFixed(1) + " GiB";
	}
	if (size < MIN_TIB) {
		return Math.round(size / MIN_GIB) + " GiB";
	}
	// TiB
	if (size < MIN_3DIGIT_TIB) {
		return parseFloat(size / MIN_TIB).toFixed(1) + " TiB";
	}
	if (size < MIN_PIB) {
		return Math.round(size / MIN_TIB) + " TiB";
	}
	// PiB
	if (size < MIN_3DIGIT_PIB) {
		return parseFloat(size / MIN_PIB).toFixed(1) + " PiB";
	}
	if (size < MIN_EIB) {
		return Math.round(size / MIN_PIB) + " PiB";
	}
	// EiB
	return parseFloat(size / MIN_EIB).toFixed(1) + " EiB";
}

/* ================================= Ajax ================================= */

/**
 * @p process_response is called with parameters response and request_status. Before @p process_response is called,
 *  the response-status is hidden, @p process_response can show it by calling `request_status.show_success(text_message)`.
 */
function do_xhr_with_status(method, url, {
	body = null,
	timeout = 0, // no timeout
	show_upload_progress = false,
	response_type = 'text', // accepted values: 'text', 'json'
	remove_previous_status = true,
	show_abort_button_after = 1500, // in milliseconds
	do_before_send = null, // function to call just before sending the request; the advantage
	                       // of this over just calling it before calling do_xhr_with_status()
	                       // is that it is called after 'Try again' is clicked
	onloadend = null, // function to call after the request completes (either by success or error)
	extra_http_headers = {}, // additional HTTP headers to set before sending request, format:
	                         // extra_https_headers: {
	                         //    'some-header': 'some value',
	                         //    'another-header': 'another value',
	                         // }
}, parent_elem_for_status, process_response) {
	const status_center_elem = elem_of('center', elem_request_status_pending());
	const show_error = (msg) => {
		const new_status = elem_request_status_error();
		new_status.innerText = msg;

		const try_again_btn = elem_with_text('a', 'Try again');
		try_again_btn.addEventListener('click', () => {
			status_center_elem.remove();
			do_xhr_with_status(method, url, {
				body,
				timeout,
				show_upload_progress,
				response_type,
				remove_previous_status,
				show_abort_button_after,
				do_before_send,
				onloadend,
				extra_http_headers,
			}, parent_elem_for_status, process_response);
		}, {passive: true});
		new_status.appendChild(elem_of('center', try_again_btn));

		remove_children(status_center_elem);
		status_center_elem.appendChild(new_status);
	};

	const xhr = new XMLHttpRequest();
	xhr.onabort = () => {
		show_error('Request aborted');
	};
	xhr.onload = () => {
		// Handle errors
		if (xhr.status !== 200) {
			let msg = '';
			// Status 400 is used for reporting request errors
			if (xhr.status !== 400) {
				msg += 'Error: ';
				msg += xhr.status;
				// HTTP/2 does not have statusText (on Chrome statusText === '' in such case but
				// standard says something about 'OK' being the default value)
				if (xhr.statusText !== '' && xhr.statusText !== 'OK') {
					msg += ' ';
					msg += xhr.statusText;
				}
			}
			// Append message only if it is a non-empty text response
			const content_type = (xhr.getResponseHeader('content-type') || '').split(' ')[0].split(';')[0];
			if (xhr.response.length > 0 && content_type === 'text/plain') {
				if (msg !== '') {
					msg += '\n';
				}
				msg += xhr.response;
			}
			show_error(msg);
			return;
		}
		// Parse response
		let invalid_init_response_type = false;
		let parsed_response;
		try {
			parsed_response = (() => {
				switch (response_type) {
					case 'text': return xhr.response;
					case 'json': return xhr.response.length === 0 ? undefined : JSON.parse(xhr.response);
					default:
						invalid_init_response_type = true;
						return null;
				}
			})();
		} catch (e) {
			console.error(e);
			show_error('Response parse error');
			return;
		}
		if (invalid_init_response_type) {
			throw Error("Invalid response_type = " + response_type);
		}

		status_center_elem.style.display = 'none';
		let showed_status = false;
		const show_status = (msg, new_status) => {
			showed_status = true;
			new_status.innerText = msg;
			status_center_elem.style.display = ''; // undo setting 'none'
			remove_children(status_center_elem);
			status_center_elem.appendChild(new_status);
		};
		const request_status = {
			show_success: (msg) => {
				show_status(msg, elem_request_status_success());
			},
			show_error: (msg) => {
				show_status(msg, elem_request_status_error());
			},
		};
		process_response.call(null, parsed_response, request_status);
		if (!showed_status) {
			status_center_elem.remove();
		}
	};
	xhr.onerror = () => {
		show_error('Network error');
	};
	xhr.ontimeout = () => {
		show_error('Request timeout');
	};
	xhr.onloadend = onloadend?.bind(null);

	let bounded_progress_elem;
	let unbounded_progress_elem;
	let progress_elem;
	let progress_info_elem;
	if (show_upload_progress) {
		xhr.upload.onprogress = (e) => {
			// e.total can be smaller that e.loaded e.g. when we select /dev/urandom
			if (e.lengthComputable && e.loaded <= e.total) {
				unbounded_progress_elem.style.display = 'none';
				bounded_progress_elem.style.display = '';
				bounded_progress_elem.value = e.loaded;
				bounded_progress_elem.max = e.total;
				progress_info_elem.innerText =
					'Sent ' + humanize_file_size(e.loaded) + ' out of ' + humanize_file_size(e.total) +
					' = ' + (Math.floor(10000 * e.loaded / e.total) / 100).toFixed(2) + '%';
			} else {
				bounded_progress_elem.style.display = 'none';
				unbounded_progress_elem.style.display = '';
				progress_info_elem.innerText = 'Sent ' + humanize_file_size(e.loaded);
			}
		}
	}

	xhr.open(method, url);
	xhr.timeout = timeout;
	xhr.responseType = 'text';
	for (const name in extra_http_headers) {
		xhr.setRequestHeader(name, extra_http_headers[name]);
	}
	// Append request-status element
	if (remove_previous_status) {
		try_remove_centered_request_status(parent_elem_for_status);
	}
	append_with_fade_in_slide_down(parent_elem_for_status, status_center_elem);
	if (show_upload_progress) {
		const request_progress = elem_with_class('div', 'request-progress');
		bounded_progress_elem = request_progress.appendChild(document.createElement('progress'));
		unbounded_progress_elem = request_progress.appendChild(document.createElement('progress'));
		progress_info_elem = request_progress.appendChild(document.createElement('div'));
		xhr.upload.onprogress({lengthComputable: false, loaded: 0, total: 0});
		status_center_elem.appendChild(request_progress);
	}
	// Abort button
	const abort_btn = elem_with_text('a', 'Abort request');
	abort_btn.addEventListener('click', () => {
		xhr.abort();
	}, {passive: true});
	abort_btn.style.display = 'none';
	setTimeout(() => { abort_btn.style.display = ''; }, show_abort_button_after);
	status_center_elem.appendChild(elem_of('center', abort_btn));

	do_before_send?.call(null);
	xhr.send(body);
}

function FormState(parent_state) {
	if (this === window) {
		throw new Error('Call as "new FormState()", not "FormState()"');
	}
	const self = this;

	self.visible_input_files = 0;
	self.parent = parent_state;
	self.propagate_up = true;

	self.apply_delta_recursively = ({visible_input_files_delta}) => {
		if (visible_input_files_delta === 0) {
			return; // nothing to update
		}
		let state = self;
		while (state != null) {
			state.visible_input_files += visible_input_files_delta;
			if (!state.propagate_up) {
				break;
			}
			state = state.parent;
		}
	}
}

function Form(title, destination_api_url, {
	css_classes = null, // no additional CSS classes
	response_type = 'json', // the same accepted values as in do_xhr_with_status()
} = {}) {
	if (this === window) {
		throw new Error('Call as "new Form()", not "Form()"');
	}
	const self = this;
	self.state = new FormState(null);

	const outer_div = elem_with_class('div', 'form-container' + (css_classes == null ? '' : ' ' + css_classes));
	outer_div.appendChild(elem_with_text('h1', title));
	self.elem = outer_div.appendChild(document.createElement('form'));

	set_common_form_fields_methods(self);

	let leave_submit_button_disabled_after_submitting = false;
	const submit_inputs = [];

	self.elem.addEventListener('submit', (event) => {
		event.preventDefault();
		// Prepare for sending
		const form_data = new FormData(self.elem);
		form_data.set('csrf_token', get_cookie('csrf_token')); // TODO: remove after backend refactor
		// Send form
		do_xhr_with_status('post', destination_api_url, {
			body: form_data,
			show_upload_progress: self.state.visible_input_files > 0,
			response_type: response_type,
			do_before_send: () => {
				for (const input of submit_inputs) {
					input.disabled = true;
				}
			},
			onloadend: () => {
				if (leave_submit_button_disabled_after_submitting) {
					// Clear all password inputs, even hidden in unselected options in a select elem
					for (const input of self.elem.querySelectorAll('input[type="password"]')) {
						input.value = '';
					}
				} else {
					for (const input of submit_inputs) {
						input.disabled = false;
					}
				}
			},
			extra_http_headers: {
				'x-csrf-token': get_cookie('csrf_token'),
			},
		}, self.elem, (response, request_status) => {
			request_status.keep_submit_buttons_disabled = () => {
				leave_submit_button_disabled_after_submitting = true;
			}
			self.success_handler.call(null, response, request_status);
		});
	});

	self.success_msg = 'Success';
	self.success_handler = (response, {
		show_success/*(msg)*/,
		show_error/*(msg)*/,
		keep_submit_buttons_disabled/*()*/
	}) => {
		show_success(self.success_msg);
	};

	self.append_submit_button = (name, {css_classes = null} = {}) => {
		const input = self.elem.appendChild(elem_with_class('input', 'btn' + (css_classes == null ? '' : ' ' + css_classes)));
		input.type = 'submit';
		input.value = name;
		submit_inputs.push(input);
		return input;
	}

	self.attach_to = (elem) => {
		elem.appendChild(outer_div);
	}
}

function set_common_form_fields_methods(self) {
	self.append = (...args) => {
		self.elem.append(...args);
	}

	self.append_field_group = (label) => {
		const div = self.elem.appendChild(elem_with_class('div', 'field-group'));
		div.appendChild(elem_with_text('label', label));
		return div;
	}

	self.append_input = (type, name, label, {required = true, disabled = false} = {}) => {
		const fg = self.append_field_group(label);
		const input = fg.appendChild(document.createElement('input'));
		input.type = type;
		input.name = name;
		input.required = required;
		input.disabled = disabled;
		return input;
	}

	self.append_input_text = (name, label, value, size, {required = true, disabled = false, trim = false, placeholder = null} = {}) => {
		const input = self.append_input('text', name, label, {required, disabled});
		input.value = value;
		input.size = size;
		if (trim) {
			input.addEventListener('change', (event) => {
				input.value = input.value.trim();
			}, {passive: true});
		}
		if (placeholder != null) {
			input.placeholder = placeholder;
		}
		return input;
	}

	self.append_input_email = (name, label, value, size, {required = true, disabled = false, trim = false, placeholder = null} = {}) => {
		const input = self.append_input('email', name, label, {required, disabled});
		input.value = value;
		input.size = size;
		if (trim) {
			input.addEventListener('change', (event) => {
				input.value = input.value.trim();
			}, {passive: true});
		}
		if (placeholder != null) {
			input.placeholder = placeholder;
		}
		return input;
	}

	self.append_input_password = (name, label, size, {required = true} = {}) => {
		const input = self.append_input('password', name, label, {required});
		input.size = size;
		return input;
	}

	self.append_input_file = (name, label, {required = true, disabled = false} = {}) => {
		self.state.apply_delta_recursively({visible_input_files_delta: 1});
		return self.append_input('file', name, label, {required, disabled});
	}

	self.append_input_hidden = (name, value) => {
		const input = self.elem.appendChild(document.createElement('input'));
		input.type = 'hidden';
		input.name = name;
		input.value = value;
		return input;
	}

	self.append_select = (name, label, {required = true, disabled = false} = {}) => {
		const fg = self.append_field_group(label);
		return new Select(name, self.state, fg, {required, disabled});
	}

	self.append_checkbox = (name, label, value) => {
		const fg = self.append_field_group(label);
		const input = fg.appendChild(document.createElement('input'));
		input.type = 'checkbox';
		input.checked = value;
		const hidden = self.append_input_hidden(name, value);
		input.addEventListener('change', () => {
			hidden.value = input.checked;
		}, {passive: true});
		return input;
	}
}

function Select(name, parent_state, field_group, {required = true, disabled = false} = {}) {
	if (this === window) {
		throw new Error('Call as "new Select()", not "Select()"');
	}
	const self = this;

	const select_elem = field_group.appendChild(document.createElement('select'));
	select_elem.name = name;
	select_elem.required = required;
	select_elem.disabled = disabled;

	let selected_value = null;
	let fieldset_options = new Map();
	let activation_callbacks = new Map();

	const selected_value_changed = (new_selected_value) => {
		let visible_input_files_delta = 0;
		if (fieldset_options.has(selected_value)) {
			const old_fieldset = fieldset_options.get(selected_value);
			old_fieldset.elem.disabled = true;
			old_fieldset.elem.style.display = 'none';
			visible_input_files_delta -= old_fieldset.state.visible_input_files;
			old_fieldset.state.propagate_up = false;
		}

		selected_value = new_selected_value;
		const activation_callback = activation_callbacks.get(new_selected_value);
		if (activation_callback != null) {
			activation_callback();
		}

		if (fieldset_options.has(new_selected_value)) {
			const new_fieldset = fieldset_options.get(new_selected_value);
			new_fieldset.elem.disabled = false;
			new_fieldset.elem.style.display = '';
			visible_input_files_delta += new_fieldset.state.visible_input_files;
			new_fieldset.state.propagate_up = true;
		}

		parent_state.apply_delta_recursively({visible_input_files_delta});
	}

	select_elem.addEventListener('change', (event) => {
		selected_value_changed(event.target.value);
	}, {passive: true});

	self.append_option = (value, label, {selected = false, on_activation = () => {}} = {}) => {
		const option = select_elem.appendChild(elem_with_text('option', label));
		option.value = value;
		option.selected = selected;

		if (activation_callbacks.has(value)) {
			throw new Error(`Cannot add a duplicate option: ${value}`);
		}
		activation_callbacks.set(value, on_activation);

		if (selected) {
			if (selected_value != null) {
				throw new Error('Cannot have two selected options at the same time!');
			}
			selected_value = value;
		} else if (selected_value == null) {
			// Need to reset it every time, because the browser selects the currently added option by default
			select_elem.value = '';
		}
		return option;
	}

	self.append_fieldset_option = (value, label, {selected = false, on_activation = () => {}}, init_fieldset) => {
		if (fieldset_options.has(value)) {
			throw new Error(`Cannot add a duplicate option: ${value}`);
		}
		const option = self.append_option(value, label, {selected, on_activation});
		const fieldset_elem = document.createElement('fieldset');
		fieldset_elem.disabled = !selected;
		fieldset_elem.style.display = selected ? '' : 'none';
		field_group.parentNode.appendChild(fieldset_elem);

		const fieldset = new Fieldset(fieldset_elem, parent_state);
		fieldset.state.propagate_up = selected;
		fieldset_options.set(value, fieldset);
		init_fieldset(fieldset);

		return option;
	}

	self.disable = () => {
		select_elem.disabled = true;
	}

	self.disable_option = (value) => {
		if (select_elem.value === value) {
			select_elem.value = '';
			selected_value_changed('');
		}
		const option = select_elem.querySelector(`:scope > option[value='${value}']`);
		option.disabled = true;
	}

	self.enable_option = (value) => {
		const option = select_elem.querySelector(`:scope > option[value='${value}']`);
		option.disabled = false;
	}
}

function Fieldset(fieldset_elem, parent_state) {
	if (this === window) {
		throw new Error('Call as "new Fieldset()", not "Fieldset()"');
	}
	const self = this;

	self.state = new FormState(parent_state);
	self.elem = fieldset_elem;

	set_common_form_fields_methods(self);
}

function snake_case_to_user_string(snake_case_str) {
	return snake_case_str[0].toUpperCase() + snake_case_str.slice(1).replaceAll('_', ' ');
}

/* ================================= Lister ================================= */

// Viewport
const get_viewport_height = (() => {
	// window.inner(Height|Width) include scrollbars, but scrollbars reduce the viewport;
	// document.documentElement.client(Height|Width) is not enough, because at least on
	// Chrome on Android and Firefox on Andorid it is not updated when the browser controls
	// get hidden e.g. because you scroll down:
	//   Controls visible:                                        Controls hidden:
	//     +----------+                                             +----------+
	//     | URL, etc.|                                             | webpage  |
	//     +----------+  -                                       -  |   ...    |
	//     | webpage  |  |                                       |  |   ...    |
	//     |   ...    |  | document.documentElement.clientHeight |  |   ...    |
	//     |   ...    |  |      (has the same value as when      |  |   ...    |
	//     +----------+  v       the controls were visible)      v  +----------+
	const viewport_prober = document.createElement('span');
	viewport_prober.style.visibility = 'hidden';
	viewport_prober.style.position = 'fixed';
	viewport_prober.style.top = 0;
	viewport_prober.style.bottom = 0;
	// document.documentElement is always valid (even for scripts inside <head>)
	document.documentElement.appendChild(viewport_prober);
	// For width set .style.left = 0, .style.right = 0 and use .clientWidth
	return () => viewport_prober.clientHeight;
})();
// Scroll: relative to viewport
function how_much_is_viewport_bottom_above_elem_bottom(elem) {
	return elem.getBoundingClientRect().bottom - get_viewport_height();
}

function Lister(elem, query_url, initial_next_query_suffix) {
	if (this === window) {
		throw new Error('Call as "new Lister()", not "Lister()"');
	}
	const self = this;
	self.elem = elem;
	self.next_query_suffix = initial_next_query_suffix;
	const modal = self.elem.closest('.modal') ?? self.elem.closest('.oldmodal');
	let fetch_lock = false;
	let is_first_fetch = true;
	let shutdown = false;

	const is_lister_detached = () => {
		return !document.body.contains(self.elem);
	};
	let do_shutdown;

	// Checks whether scrolling down is (almost) impossible
	const need_to_fetch_more = () => {
		return how_much_is_viewport_bottom_above_elem_bottom(self.elem) <= 300;
	}

	if (self.process_first_api_response == null) {
		throw new Error('Unset self.process_first_api_response');
	}
	if (self.process_api_response == null) {
		throw new Error('Unset self.process_api_response');
	}

	self.fetch_more = () => {
		if (fetch_lock || shutdown) {
			return;
		}
		if (is_lister_detached()) {
			do_shutdown();
			return;
		}

		fetch_lock = true;

		do_xhr_with_status('get', query_url + self.next_query_suffix, {
			response_type: 'json',
		}, self.elem.parentNode, (data) => {
				if (is_first_fetch) {
					is_first_fetch = false;
					self.process_first_api_response(data.list);
				} else if (data.list.length !== 0) {
					self.process_api_response(data.list);
				}

				if (!data.may_be_more) {
					do_shutdown(); // No more data to load
					fetch_lock = false;
					return;
				}

				fetch_lock = false;
				if (need_to_fetch_more()) {
					self.fetch_more();
				}
		});
	};

	const scroll_or_resize_event_handler = () => {
		if (is_lister_detached()) {
			do_shutdown();
			return;
		}
		if (need_to_fetch_more()) {
			setTimeout(self.fetch_more, 0); // schedule fetching more without blocking
		}
	}

	// Start listening for scroll and resize events
	const elem_to_listen_on_scroll = modal === null ? document : modal;
	elem_to_listen_on_scroll.addEventListener('scroll', scroll_or_resize_event_handler, {passive: true});
	window.addEventListener('resize', scroll_or_resize_event_handler, {passive: true});

	do_shutdown = () => {
		shutdown = true;
		elem_to_listen_on_scroll.removeEventListener('scroll', scroll_or_resize_event_handler);
		window.removeEventListener('resize', scroll_or_resize_event_handler);
	};

	if (need_to_fetch_more()) {
		self.fetch_more();
	}
}

/* ================================= History and navigation ================================= */

/* The model is as follows:
 * - each page has a distinct URL and a corresponding View object
 * - View objects do not nest, they form a stack -- just like the browser history
 * - each Modal is pinned to some View
 * - Modals pinned to the same View form a stack
 * - View are persistent i.e. their existence is saved in the browser history e.g. they survive page reload
 * - Modals are volatile i.e. they do not survive page reload
 * - when View disappears it hides its Modals, the same holds for reappearing
 * - when View is removed, all its Modals are also removed
 *
 * E.g. hierarchy state:
 * - View a
 *   - Modal x
 *   - Modal y
 * - View b
 * - View c
 *   - Modal z
 *
 * If the current view in history was View b, then:
 * - going forward would make View c and Modal z appear
 * - going backward would make View b disappear, so that View a, Modal x and Modal y are visible
 * - going backward and forward and then forward would make View c and Modal z appear
 * - reloading and going forwards would make only View c appear
 */

const History = (() => {
	let persistent_state;
	// view_id_prefix is needed in case we go backwards in history and encounter view_id in
	// history.state that has the same value as some View that exists in the DOM tree as an
	// newer, other history entry: we have View with id=1, View with id=4, we go back to id=1
	// and then go back once again and encounter id=4 in history.state
	const view_id_prefix = window.crypto.getRandomValues(new Uint32Array(4)).toString() + '-';
	let view_id_num = 0;
	const get_next_view_id = () => {
		const id = view_id_prefix + view_id_num;
		++view_id_num;
		return id;
	};

	const get_view_elem = (view_id) => document.querySelector(`[view_id="${view_id}"]`);
	const get_current_view_elem = () => get_view_elem(history.state.view_id);

	const hide_views_covering_elem = (view_elem) => {
		let elem = view_elem.nextElementSibling;
		while (elem != null) {
			const next_elem = elem.nextElementSibling;
			if (elem.hasAttribute('view_id')) {
				if (elem.classList.contains('display-none')) {
					break; // All views above are hidden
				}
				elem.classList.add('display-none');
			}
			elem = next_elem;
		}
	};
	const show_views_covered_by = (view_elem) => {
		let elem = view_elem.previousElementSibling;
		while (elem != null) {
			const next_elem = elem.previousElementSibling;
			if (elem.hasAttribute('view_id')) {
				if (!elem.classList.contains('display-none')) {
					break; // All views below are visible
				}
				elem.classList.remove('display-none');
			}
			elem = next_elem;
		}
	};

	const delete_hidden_views_forward_in_history = () => {
		const curr_view_elem = get_current_view_elem();
		if (curr_view_elem != null) {
			let elem = curr_view_elem.nextElementSibling;
			while (elem != null) {
				const next_elem = elem.nextElementSibling;
				if (elem.hasAttribute('view_id')) {
					elem.remove();
				}
				elem = next_elem;
			}
		}
	};

	const current_view_url_state_list_from_url_hash = (url_hash) => {
		const list = [];
		let cursor = 0;

		let beg = 0;
		const end = url_hash.length;
		while (beg < end) {
			assert(url_hash[beg] === '#');
			++beg; // Skip hash
			let next_beg = url_hash.indexOf('#', beg);
			if (next_beg === -1) {
				next_beg = end;
			}
			list.push(decodeURIComponent(url_hash.substring(beg, next_beg)));
			beg = next_beg;
		}

		const list_to_url_hash = (list) => {
			let url_hash = '';
			for (const elem of list) {
				url_hash += '#';
				url_hash += encodeURIComponent(elem);
			}
			return url_hash;
		};

		const update_history_state = () => {
			history.replaceState({
				persistent_state: persistent_state,
				view_id: history.state.view_id,
			}, '', History.current_url_without_hash() + list_to_url_hash(list));
		};

		const url_without_hash = (url) => {
			let hash_pos = url.indexOf('#');
			if (hash_pos === -1) {
				hash_pos = url.length;
			}
			return url.substring(0, hash_pos);
		};

		return {
			get_curr_elem_and_advance_cursor: () => {
				if (cursor === list.length) {
					return [null, cursor];
				}
				const res = [list[cursor], cursor];
				++cursor;
				return res;
			},
			resize_and_append: (new_length, elem) => {
				assert(new_length <= list.length);
				list.splice(new_length, list.length - new_length, elem);
				cursor = new_length + 1;
				update_history_state();
			},
			other_url_with_state: (url) => {
				return url_without_hash(url) + list_to_url_hash(list);
			},
			other_url_with_state_after_resize_and_append: (url, new_length, elem) => {
				assert(new_length <= list.length);
				let new_list = list.slice(0, new_length);
				new_list.push(elem);
				return url_without_hash(url) + list_to_url_hash(new_list);
			},
		};
	};

	window.onpopstate = (event) => {
		if (event.state == null) {
			// This event come from an user, e.g. they modified manually an url like "/#test"
			return location.reload(); // we are helpless when event.state is null
		}
		const view_elem_to_focus = get_view_elem(event.state.view_id);
		if (view_elem_to_focus == null) {
			// TODO: dispatch url in js and present appropriate view instead of reloading
			// (this will be possible after refactoring UI in such a way that url dispatching is done in UI i.e. in js)
			return location.reload();
		}
		view_elem_to_focus.classList.remove('display-none');
		hide_views_covering_elem(view_elem_to_focus);
		show_views_covered_by(view_elem_to_focus); // important if e.g. somebody goes forward by 2 entries: history.go(2);
		History.current_view_url_state_list = current_view_url_state_list_from_url_hash(window.location.hash);
	};

	return {
		init: (make_default_persistent_state) => {
			if (history.state != null) {
				persistent_state = history.state.persistent_state;
				return persistent_state;
			}
			persistent_state = make_default_persistent_state();
			history.replaceState({
				persistent_state: persistent_state,
			}, '', document.URL);
			History.current_view_url_state_list = current_view_url_state_list_from_url_hash(window.location.hash);
			return persistent_state;
		},
		push: (view_elem, new_location) => {
			delete_hidden_views_forward_in_history();
			const view_id = get_next_view_id();
			view_elem.setAttribute('view_id', view_id);
			history.pushState({
				persistent_state: persistent_state,
				view_id: view_id,
			}, '', new_location);
			History.current_view_url_state_list = current_view_url_state_list_from_url_hash(window.location.hash);
			old_url_hash_parser.assign(window.location.hash); // TODO: remove after refactor
		},
		replace_current: (view_elem, new_location) => {
			const view_id = view_elem.getAttribute('view_id') ?? get_next_view_id();
			view_elem.setAttribute('view_id', view_id);
			history.replaceState({
				persistent_state: persistent_state,
				view_id: view_id,
			}, '', new_location);
			History.current_view_url_state_list = current_view_url_state_list_from_url_hash(window.location.hash);
			old_url_hash_parser.assign(window.location.hash); // TODO: remove after refactor
		},
		get_current_view_elem: get_current_view_elem,
		is_view_elem: (elem) => elem.hasAttribute('view_id'),
		current_url_without_hash: () => document.URL.substring(0, document.URL.length - window.location.hash.length),
		current_view_url_state_list: current_view_url_state_list_from_url_hash(window.location.hash),
	};
})();

function sim_template(params, server_response_end_ts) {
	const history_persistent_state = History.init(() => {
		let server_time_offset;
		const entries = window.performance.getEntriesByType('navigation');
		if (entries.length === 0) {
			// For Safari and other unsupporting browsers
			server_time_offset = server_response_end_ts - window.performance.timing.responseStart;
		} else {
			const curr_time = Date.now();
			// floor() in order to never report slightly future time
			const current_server_time = Math.floor(server_response_end_ts - entries[0].responseStart + performance.now());
			server_time_offset = current_server_time - curr_time;
		}
		return {
			server_time_offset: server_time_offset,
		};
	});
	// Close modal or hide the current view on pressing Escape
	document.addEventListener('keydown', (event) => {
		if (event.key !== 'Escape') {
			return;
		}
		const curr_view_elem = History.get_current_view_elem();
		const potential_modal = curr_view_elem.lastElementChild;
		if (potential_modal != null && (potential_modal.classList.contains('modal') || potential_modal.classList.contains('oldmodal'))) {
			potential_modal.remove(); // remove top modal inside the current view
		} else if (curr_view_elem.classList.contains('modal') || curr_view_elem.classList.contains('oldmodal')) { // cannot a hide non-modal view
			history.back(); // hide the current view
		}
	}, {passive: true});
	// TODO: remove body_view_elem after refactoring every view to use View
	let body_view_elem = document.body.appendChild(elem_with_class('span', 'body_view_elem'));
	body_view_elem.style.height = 0;
	body_view_elem.style.width = 0;
	History.replace_current(body_view_elem, window.location.href);

	window.current_server_time = () => {
		return new Date(Date.now() + history_persistent_state.server_time_offset);
	};
	window.ServerClock = (() => {
		const listeners = new Array();
		function run_listeners() {
			for (const listener of listeners) {
				listener();
			}
		};
		function run_listeners_every_second() {
			run_listeners();
			setTimeout(run_listeners_every_second, 1001 - current_server_time().getMilliseconds()); // 1001 because of possible rounding error
			                                                                                        // that would make the handler run e.g. on 37.999
			                                                                                        // instead of 38.000 (at least in Firefox 89)
		}
		run_listeners_every_second();
		// When going back and forth in history (e.g. using Alt+arrow), some browsers (e.g. firefox) reuse the old document
		// (sim_template() will not be called) and generate only pageshow event. The below event handler makes sure that
		// the clock listeners are updated with actual time when the old document is rendered.
		window.addEventListener('pageshow', function(event) {
			// Make sure the listeners are updated with actual time
			run_listeners();
			// setTimeout() from run_listeners_every_second() may not happen just after the next second starts, because
			// timers (set up using setTimeout) seem to be duration based instead of time-point based i.e. this could happen:
			// 1. 37.845 (server time's seconds): 'pageshow' is handled and run_listeners() is called
			// 2. 38.367 (server time's seconds): timeout set by run_listeners_every_second() elapsed and run_listeners() is called
			// In the above scenario, there is no run_listeners() call just after 38.000 (server time's second) passes,
			// so to ensure there will be such call, we schedule it here
			setTimeout(run_listeners, 1001 - current_server_time().getMilliseconds()); // 1001 because of possible rounding error
			                                                                           // that would make the handler run e.g. on 37.999
			                                                                           // instead of 38.000 (at least in Firefox 89)
		}, {passive: true});

		return {
			add_listener: (listener) => {
				listeners.push(listener);
				listener();

			},
			remove_listener: (listener) => {
				const pos = listeners.indexOf(listener);
				if (pos !== -1) {
					listeners.splice(pos, 1);
				}
			},
		}
	})();

	handle_session_change(params);
}

function handle_session_change(params) {
	window.global_capabilities = params.capabilities;
	window.is_signed_in = () => signed_user_id != null;

	window.signed_user_id = params.session?.user_id;
	window.signed_user_type = params.session?.user_type;

	const navbar = (() => {
		const navbar = document.querySelector('.navbar');
		if (navbar != null) {
			remove_children(navbar);
			return navbar;
		}
		return document.body.appendChild(elem_with_class('div', 'navbar'));
	})();
	navbar.appendChild(elem_with_class_and_text('a', 'brand', 'Sim beta')).href = url_main_page();
	if (global_capabilities.contests.ui_view) {
		// TODO: this requires jQuery
		$(navbar).append(a_view_button(url_contests(), 'Contests', undefined, contest_chooser));
	}
	if (global_capabilities.problems.ui_view) {
		navbar.appendChild(elem_with_text('a', 'Problems')).href = url_problems();
	}
	if (global_capabilities.users.ui_view) {
		navbar.appendChild(elem_with_text('a', 'Users')).href = url_users();
	}
	if (global_capabilities.submissions.ui_view) {
		navbar.appendChild(elem_with_text('a', 'Submissions')).href = url_submissions();
	}
	if (global_capabilities.jobs.ui_view) {
		navbar.appendChild(elem_with_text('a', 'Jobs')).href = url_jobs();
	}
	if (global_capabilities.logs.ui_view) {
		navbar.appendChild(elem_with_text('a', 'Logs')).href = url_logs();
	}

	// Navigation bar clock
	const clock = elem_with_id('time', 'clock');
	ServerClock.add_listener(() => {
		const server_time = current_server_time();
		let h = server_time.getHours();
		let m = server_time.getMinutes();
		let s = server_time.getSeconds();
		h = (h < 10 ? '0' : '') + h;
		m = (m < 10 ? '0' : '') + m;
		s = (s < 10 ? '0' : '') + s;
		const tzo = -server_time.getTimezoneOffset();
		remove_children(clock);
		clock.append(h, ':', m, ':', s, elem_with_text('sup', 'UTC' + (tzo >= 0 ? '+' : '') + tzo / 60));
	});
	navbar.appendChild(clock);
	window.onerror = (() => {
		let error_count = 0;
		const errors = [];
		const error_counter_elem = elem_link_with_class_to_modal('error_counter', '', () => {
			const modal = new Modal();
			modal.content_elem.appendChild(elem_with_text('h2', 'You are seeing this because something went wrong...'));
			const issues_link = elem_with_text('a', 'https://github.com/varqox/sim-project/issues');
			issues_link.href = 'https://github.com/varqox/sim-project/issues';
			issues_link.target = '_blank';
			issues_link.rel = 'noopener noreferrer';
			modal.content_elem.appendChild(elem_of('p', 'Please report a bug by going to ', issues_link, ' and creating an issue with the following log:'));
			const error_log_str = errors.join('\n=================================================\n\n');
			const code_elem = elem_with_class_and_text('code', 'error_log', error_log_str);
			code_elem.style.wordWrap = 'wrap';
			modal.content_elem.append(elem_of('div', copy_to_clipboard_btn(false, 'Copy log to clipboard', () => error_log_str)[0])); // TODO: refactor copy_to_clipboard_btn
			modal.content_elem.appendChild(code_elem);
		});
		return (message, source, lineno, colno, error) => {
			errors.push(`${message}\nStack trace:\n${error.stack}`);
			window.x = errors;
			++error_count;
			error_counter_elem.textContent = `JS errors: ${error_count}`;
			clock.before(error_counter_elem);
		};
	})();

	if (params.session) {
		const dropdown_menu = navbar.appendChild(elem_with_class('div', 'dropmenu down'));
		const toggler = dropdown_menu.appendChild(elem_with_class('a', 'user dropmenu-toggle'));
		toggler.appendChild(elem_with_text('strong', params.session.username));

		const ul = dropdown_menu.appendChild(document.createElement('ul'));
		ul.appendChild(elem_with_text('a', 'My profile')).href = url_user(params.session.user_id);
		ul.appendChild(elem_with_text('a', 'Edit profile')).href = url_user_edit(params.session.user_id);
		ul.appendChild(elem_with_text('a', 'Change password')).href = url_change_user_password(params.session.user_id);
		ul.appendChild(elem_link_to_view('Sign out', sign_out, url_sign_out));
	} else {
		navbar.appendChild(elem_link_to_view(elem_with_text('strong', 'Sign in'), sign_in, url_sign_in));
		navbar.appendChild(elem_link_to_view('Sign up', sign_up, url_sign_up));
	}
}

/* ================================= Modal ================================= */

function ModalBase() {
	if (this === window) {
		throw new Error('Call as "new ModalBase()", not "ModalBase()"');
	}
	const self = this;
	const modal_elem = elem_with_class('div', 'modal');
	const centering_div = modal_elem.appendChild(elem_with_class('div', 'centering'));
	const modal_window_div = centering_div.appendChild(elem_with_class('div', 'window'));
	const close_elem = modal_window_div.appendChild(elem_with_class('span', 'close'));
	const content_elem = modal_window_div.appendChild(document.createElement('div'));
	// Prevent modal from shrinking (this is annoying e.g. when changing tabs in a tab menu)
	let vertical_scroll_appeared = false;
	let horizontal_scroll_appeared = false;
	const update_min_height_and_min_width = () => {
		const vertical_scrollbar_width = modal_elem.offsetWidth - modal_elem.clientWidth;
		if (vertical_scrollbar_width > 0) {
			if (!vertical_scroll_appeared) {
				vertical_scroll_appeared = true;
				// Pin the vertical scrollbar, so we won't get glitches (the whole modal
				// moves slightly to the right if the scrollbar disappears)
				modal_elem.style.overflowY = 'scroll';
			}
		}
		const horizontal_scrollbar_height = modal_elem.offsetHeight - modal_elem.clientHeight;
		if (horizontal_scrollbar_height > 0) {
			if (!horizontal_scroll_appeared) {
				horizontal_scroll_appeared = true;
				// Pin the horizontal scrollbar, so we won't get glitches (the whole modal
				// moves slightly to the top if the scrollbar disappears)
				modal_elem.style.overflowX = 'scroll';
			}
		}
		// console.log('update_min_height_and_min_width()'); // TODO: use this to track down remaining event listeners
		const bounding_client_rect = modal_window_div.getBoundingClientRect();
		// bounding_client_rect will be bounded by previous min-height and min-width,
		// so there is no need for using max(old_val, new_val). This way, resizing the browser
		// window may shrink modals, which is good; min() is used to ensure that modal is not
		// larger than the viewport unless necessary.
		const computed_styles = getComputedStyle(modal_window_div);
		modal_window_div.style.minWidth = `min(${bounding_client_rect.width}px, 100% - ${computed_styles['margin-left']} - ${computed_styles['margin-right']})`;
		// We cannot use 100% because height of the parent element is not specified explicitly,
		// so 100% would be treated as 0 and it is not what we need. Also 100vh is not what
		// we need because it does not exclude scrollbars. min() is used to ensure that
		// modal is not larger than the viewport unless necessary.
		modal_window_div.style.minHeight = `min(${bounding_client_rect.height}px, ${modal_elem.clientHeight}px - ${computed_styles['margin-top']} - ${computed_styles['margin-bottom']})`;
	};
	update_min_height_and_min_width();
	const mo = new MutationObserver((mutation_records) => {
		for (const record of mutation_records) {
			if (record.target === modal_window_div && record.type === 'attributes') {
				continue; // ignore changes to modal_window_div's attributes, e.g. caused by the below min-height and min-width change
			}
			// TODO: check if this object is also destroyed
			update_min_height_and_min_width();
			break;
		}
	});
	mo.observe(modal_window_div, {subtree: true, childList: true, attributes: true, characterData: true});
	// TODO: remove this listener when the modal is removed (but don't remove this listener before the modal becomes attached)
	window.addEventListener('resize', update_min_height_and_min_width, {passive: true});

	const do_close = () => {
		if (History.is_view_elem(modal_elem) && modal_elem === History.get_current_view_elem()) {
			history.back();
			return;
		}
		modal_elem.remove();
	};
	close_elem.addEventListener('click', do_close, {passive: true});
	centering_div.addEventListener('click', (event) => {
		if (event.target === centering_div) {
			do_close();
		}
	}, {passive: true});

	self.modal_elem = modal_elem;
	self.content_elem = content_elem;
}

function set_api_interface(modal_or_view, content_elem) {
	modal_or_view.get_from_api = (url) => {
		return new Promise((resolve, reject, request_handler = (response, request_status) => response) => {
			do_xhr_with_status('get', url, {
				response_type: 'json',
			}, content_elem, (response, request_status) => {
				resolve(request_handler(response, request_status));
			});
		});
	};
	modal_or_view.post_to_api = (url, form_data, request_handler = (response, request_status) => response) => {
		form_data.set('csrf_token', get_cookie('csrf_token')); // TODO: remove after backend refactor
		return new Promise((resolve, reject) => {
			do_xhr_with_status('post', url, {
				body: form_data,
				response_type: 'json',
				extra_http_headers: {
					'x-csrf-token': get_cookie('csrf_token'),
				},
			}, content_elem, (response, request_status) => {
				resolve(request_handler(response, request_status));
			});
		});
	};
}

function Modal() {
	if (this === window) {
		throw new Error('Call as "new Modal()", not "Modal()"');
	}
	const self = this;
	ModalBase.call(self);
	set_api_interface(self, self.content_elem);
	append_with_fade_in(History.get_current_view_elem(), self.modal_elem);
}

function elem_link_to_modal(contents, modal_func) {
	const elem = elem_of('a', contents);
	elem.href = 'javascript:;';
	elem.addEventListener('click', (event) => {
		event.preventDefault();
		modal_func();
	});
	return elem;
}

function elem_link_with_class_to_modal(classes, contents, modal_func) {
	const elem = elem_link_to_modal(contents, modal_func);
	elem.className = classes;
	return elem;
}

function View(new_window_location) {
	if (this === window) {
		throw new Error('Call as "new View()", not "View()"');
	}
	// Check if we want to retain the url state
	{
		const new_location_with_url_state = History.current_view_url_state_list.other_url_with_state(new_window_location);
		if (document.URL.endsWith(new_location_with_url_state)) {
			new_window_location = new_location_with_url_state;
		}
	}
	const self = this;
	if (document.querySelector('div.view') == null) {
		const div = document.body.appendChild(elem_with_class('div', 'view'));
		self.view_elem = div;
		self.content_elem = div;
		History.replace_current(self.view_elem, new_window_location);
	} else {
		const modal = new ModalBase();
		append_with_fade_in(document.body, modal.modal_elem);
		modal.modal_elem.classList.add('view');
		self.view_elem = modal.modal_elem;
		self.content_elem = modal.content_elem;
		History.push(self.view_elem, new_window_location);
	}
	set_api_interface(self, self.content_elem);
};

function elem_link_to_view(contents, view_func, url_func, ...view_and_url_func_args) {
	const elem = elem_of('a', contents);
	elem.href = url_func(...view_and_url_func_args);
	elem.addEventListener('click', (event) => {
		event.preventDefault();
		view_func(...view_and_url_func_args);
	});
	return elem;
}

function elem_link_with_class_to_view(classes, contents, view_func, url_func, ...view_and_url_func_args) {
	const elem = elem_link_to_view(contents, view_func, url_func, ...view_and_url_func_args);
	elem.className = classes;
	return elem;
}

function TabMenuBuilder() {
	if (this === window) {
		throw new Error('Call as "new TabMenuBuilder()", not "TabMenuBuilder()"');
	}
	const self = this;
	const outer_div = elem_with_class('div', 'tab_menu');
	const tabs = [];

	self.active_tab_changed_listener = (tab_name) => {};

	self.add_tab = (tab_name, tab_builder) => {
		tabs.push({name: tab_name, builder: tab_builder});
	};

	self.build_and_append_to = (elem) => {
		elem.appendChild(outer_div);
		let [url_state_elem_name, url_state_idx] = History.current_view_url_state_list.get_curr_elem_and_advance_cursor();
		// Check if such tab exists
		(() => {
			for (const tab of tabs) {
				if (tab.name === url_state_elem_name) {
					return; // exists
				}
			}
			url_state_elem_name = null; // does not exist
		})();

		const tabs_div = outer_div.appendChild(elem_with_class('div', 'tabs'));
		const tab_content_div = outer_div.appendChild(elem_with_class('div', 'tab_content'))
		let active_tab_elem = null;
		for (const [idx, tab] of tabs.entries()) {
			const tab_elem = tabs_div.appendChild(elem_with_text('a', tab.name));
			tab_elem.href = History.current_view_url_state_list.other_url_with_state_after_resize_and_append(document.URL, url_state_idx, tab.name);

			const activate_tab = () => {
				tab_elem.classList.add('active');
				active_tab_elem = tab_elem;
				self.active_tab_changed_listener(tab.name);
				tab.builder(tab_content_div);
			};
			// TODO: make sure this event handler is removed on tabmenu removal
			tab_elem.addEventListener('click', (event) => {
				event.preventDefault();
				active_tab_elem.classList.remove('active');
				remove_children(tab_content_div);
				History.current_view_url_state_list.resize_and_append(url_state_idx, tab.name);
				activate_tab();
			});

			if ((url_state_elem_name == null && idx === 0) || (url_state_elem_name != null && tab.name === url_state_elem_name)) {
				if (url_state_elem_name == null) {
					History.current_view_url_state_list.resize_and_append(url_state_idx, tab.name);
				}
				activate_tab();
			}
		}
	};
}

/* ================================= Pages ================================= */

function main_page() {
	const view = new View('/');
	const img = document.createElement('img');
	img.src = url_sim_logo_img();
	img.width = 260;
	img.height = 336;
	img.alt = '';

	const welcome_p = elem_with_text('p', 'Welcome to Sim');
	welcome_p.style.fontSize = '30px'

	view.content_elem.appendChild(elem_of('center', img, welcome_p, document.createElement('hr'), elem_with_text('p', 'Sim is an open source platform for carrying out algorithmic contests')));
}

async function add_user() {
	const view = new View(url_users_add());
	const form = new Form('Add user', url_api_users_add());
	const type = form.append_select('type', 'Type');
	if (global_capabilities.users.add_admin) {
		type.append_option('admin', 'Admin');
	}
	if (global_capabilities.users.add_teacher) {
		type.append_option('teacher', 'Teacher');
	}
	if (global_capabilities.users.add_normal_user) {
		type.append_option('normal', 'Normal', {selected: true});
	}

	form.append_input_text('username', 'Username', '', 24, {trim: true});
	form.append_input_text('first_name', 'First name', '', 24, {trim: true});
	form.append_input_text('last_name', 'Last name', '', 24, {trim: true});
	form.append_input_email('email', 'Email', '', 24, {trim: true});
	form.append_input_password('password', 'Password', 24, {required: false});
	form.append_input_password('password_repeated', 'Password (repeat)', 24, {required: false});
	form.append_submit_button('Add user', {css_classes: 'blue'});
	form.attach_to(view.content_elem);
}

async function edit_user(user_id) {
	const view = new View(url_user_edit(user_id));
	const user = await view.get_from_api(url_api_user(user_id));
	const form = new Form('Edit user', url_api_user_edit(user.id));

	const type = form.append_select('type', 'Type');
	if (!user.capabilities.change_type) {
		type.disable();
	}
	if (user.capabilities.make_admin || user.type === 'admin') {
		type.append_option('admin', 'Admin', {selected: user.type === 'admin'});
	}
	if (user.capabilities.make_teacher || user.type === 'teacher') {
		type.append_option('teacher', 'Teacher', {selected: user.type === 'teacher'});
	}
	if (user.capabilities.make_normal || user.type === 'normal') {
		type.append_option('normal', 'Normal', {selected: user.type === 'normal'});
	}

	form.append_input_text('username', 'Username', user.username, 24, {
		disabled: !user.capabilities.edit_username,
		trim: true
	});
	form.append_input_text('first_name', 'First name', user.first_name, 24, {
		disabled: !user.capabilities.edit_first_name,
		trim: true
	});
	form.append_input_text('last_name', 'Last name', user.last_name, 24, {
		disabled: !user.capabilities.edit_last_name,
		trim: true
	});
	form.append_input_email('email', 'Email', user.email, 24, {
		disabled: !user.capabilities.edit_email,
		trim: true
	});
	form.append_submit_button('Update', {css_classes: 'blue'});
	form.attach_to(view.content_elem);
}

async function change_user_password(user_id) {
	const view = new View(url_change_user_password(user_id));
	const user = await view.get_from_api(url_api_user(user_id));
	const is_me = (user.id === signed_user_id);
	const title = is_me ? 'Change my password'
	                    : 'Change password of\n' + user.first_name + ' ' + user.last_name;
	const form = new Form(title, url_api_user_change_password(user.id));
	if (!user.capabilities.change_password_without_old_password) {
		form.append_input_password('old_password', 'Old password', 24, {required: false});
	}
	form.append_input_password('new_password', 'New password', 24, {required: false});
	form.append_input_password('new_password_repeated', 'New password (repeat)', 24, {required: false});
	form.append_submit_button(title, {css_classes: 'blue'});
	form.success_msg = 'Password changed';
	form.attach_to(view.content_elem);
}

async function delete_user(user_id) {
	const view = new View(url_user_delete(user_id));
	const user = await view.get_from_api(url_api_user(user_id));
	const is_me = (user.id === signed_user_id);
	const title = is_me ? 'Delete account' : 'Delete user\n' + user.first_name + ' ' + user.last_name;
	const form = new Form(title, url_api_user_delete(user.id), {
		css_classes: 'with-notice',
	});
	if (is_me) {
		form.append(elem_of('p', 'You are going to delete your account. As it cannot be undone, you have to confirm it with your password.'));
	} else {
		form.append(elem_of('p',
			'You are going to delete the user ',
			a_view_button(url_user(user.id), user.username, undefined, view_user.bind(null, true, user.id)), // TODO: refactor a_view_button
			'. As it cannot be undone, you have to confirm it with YOUR password.'));
	}
	form.append_input_password('password', 'Your password', 24, {required: false});
	form.append_submit_button(title, {css_classes: 'red'});
	form.success_handler = (response, {show_success, keep_submit_buttons_disabled}) => {
		keep_submit_buttons_disabled();
		show_success('Scheduled deletion');
		if (!is_me) {
			view_job(true, response.job_id);
		}
	};
	form.attach_to(view.content_elem);
}

async function merge_user(user_id) {
	const view = new View(url_user_merge_into_another(user_id));
	const user = await view.get_from_api(url_api_user(user_id));
	const form = new Form('Merge into another user', url_api_user_merge_into_another(user.id), {
		css_classes: 'with-notice',
	});
	form.append(elem_of('p', 'The user ',
		a_view_button(url_user(user.id), user.username, undefined, view_user.bind(null, true, user.id)), // TODO: refactor a_view_button
		' is going to be deleted. All their problems, submissions, jobs and accesses to contests will be transfered to the target user.',
		document.createElement('br'),
		'As this cannot be undone, you have to confirm this with your password.'));
	form.append_input_text('target_user_id', 'Target user ID', '', 6, {trim: true});
	form.append_input_password('password', 'YOUR password', 24, {required: false});
	form.append_submit_button('Merge user ' + user.id, {css_classes: 'red'});
	form.success_handler = (response, {show_success, keep_submit_buttons_disabled}) => {
		keep_submit_buttons_disabled();
		show_success('Merging has been scheduled');
		view_job(true, response.job_id);
	};
	form.attach_to(view.content_elem);
}

async function sign_in() {
	const view = new View(url_sign_in());
	const form = new Form('Sign in', url_api_sign_in());
	form.append_input_text('username', 'Username', '', 24, {trim: true});
	form.append_input_password('password', 'Password', 24, {required: false});
	form.append_checkbox('remember_for_a_month', 'Remember for a month', false);
	const old_csrf_token = get_cookie('csrf_token');
	form.success_handler = (response, {show_success, show_error, keep_submit_buttons_disabled}) => {
		const csrf_token = get_cookie('csrf_token');
		if ((csrf_token === '' || csrf_token === old_csrf_token) && !location.href.startsWith('https://')) {
			show_error('Cannot set session cookies due to non-HTTPS connection.\nServer administrator should have ensured that the site is available through HTTPS.');
		} else {
			show_success('Signed in successfully');
			keep_submit_buttons_disabled();
			handle_session_change(response);
		}
	};
	form.append_submit_button('Sign in', {css_classes: 'blue'});
	form.attach_to(view.content_elem);
}

async function sign_up() {
	const view = new View(url_sign_up());
	const form = new Form('Sign up', url_api_sign_up());
	form.append_input_text('username', 'Username', '', 24, {trim: true});
	form.append_input_text('first_name', 'First name', '', 24, {trim: true});
	form.append_input_text('last_name', 'Last name', '', 24, {trim: true});
	form.append_input_email('email', 'Email', '', 24, {trim: true});
	form.append_input_password('password', 'Password', 24, {required: false});
	form.append_input_password('password_repeated', 'Password (repeat)', 24, {required: false});
	const old_csrf_token = get_cookie('csrf_token');
	form.success_handler = (response, {show_success, show_error, keep_submit_buttons_disabled}) => {
		const csrf_token = get_cookie('csrf_token');
		if ((csrf_token === '' || csrf_token === old_csrf_token) && !location.href.startsWith('https://')) {
			show_error('Cannot set session cookies due to non-HTTPS connection.\nServer administrator should have ensured that the site is available through HTTPS.');
		} else {
			show_success('Signed up successfully');
			keep_submit_buttons_disabled();
			handle_session_change(response);
		}
	};
	form.append_submit_button('Sign up', {css_classes: 'blue'});
	form.attach_to(view.content_elem);
}

async function sign_out() {
	const view = new View(url_sign_out());
	view.content_elem.appendChild(elem_of('center', elem_with_text('h1', 'Signing out')));
	view.post_to_api(url_api_sign_out(), new FormData(), (response, request_status) => {
		request_status.show_success("Signed out");
		handle_session_change(response);
	});
}

function UsersLister(elem, query_url) {
	if (this === window) {
		throw new Error('Call as "new UsersLister()", not "UsersLister()"');
	}
	const self = this;
	self.process_first_api_response = (list) => {
		if (list.length === 0) {
			self.elem.parentNode.appendChild(elem_with_text('p', 'There are no users to show...'));
			return;
		}

		const thead = document.createElement('thead');
		thead.appendChild(elem_with_text('th', 'Id'));
		thead.appendChild(elem_with_class_and_text('th', 'username', 'Username'));
		thead.appendChild(elem_with_class_and_text('th', 'first-name', 'First name'));
		thead.appendChild(elem_with_class_and_text('th', 'last-name', 'Last name'));
		thead.appendChild(elem_with_class_and_text('th', 'email', 'Email'));
		thead.appendChild(elem_with_class_and_text('th', 'type', 'Type'));
		thead.appendChild(elem_with_class_and_text('th', 'actions', 'Actions'));
		self.elem.appendChild(thead);
		self.tbody = document.createElement('tbody');
		self.elem.appendChild(self.tbody);

		self.process_api_response(list);
	};
	self.process_api_response = (list) => {
		self.next_query_suffix = '/id>/' + list[list.length - 1].id;

		const document_fragment = document.createDocumentFragment();
		for (const user of list) {
			const row = document.createElement('tr');
			row.appendChild(elem_with_text('td', user.id));
			row.appendChild(elem_with_text('td', user.username));
			row.appendChild(elem_with_text('td', user.first_name));
			row.appendChild(elem_with_text('td', user.last_name));
			row.appendChild(elem_with_text('td', user.email));
			row.appendChild(elem_with_class_and_text('td', user.type, snake_case_to_user_string(user.type)));

			const td = document.createElement('td');
			td.append.apply(td, ActionsToHTML.user(user));
			row.appendChild(td);
			document_fragment.appendChild(row);
		}
		self.tbody.appendChild(document_fragment);
	};

	Lister.call(self, elem, query_url, '');
}

function list_users() {
	const view = new View(url_users());
	view.content_elem.appendChild(elem_with_text('h1', 'Users'));
	if (global_capabilities.users.add_user) {
		view.content_elem.appendChild(elem_link_with_class_to_view('btn', 'Add user', add_user, url_users_add));
	}

	const retab = (url, elem) => {
		const table = elem.appendChild(elem_with_class('table', 'users stripped'));
		new UsersLister(table, url);
	};

	const tabmenu = new TabMenuBuilder();
	if (global_capabilities.users.list_all.query_all) {
		tabmenu.add_tab('All', retab.bind(null, url_api_users()));
	}
	if (global_capabilities.users.list_all.query_with_type_admin) {
		tabmenu.add_tab('Admins', retab.bind(null, url_api_users_with_type('admin')));
	}
	if (global_capabilities.users.list_all.query_with_type_teacher) {
		tabmenu.add_tab('Teachers', retab.bind(null, url_api_users_with_type('teacher')));
	}
	if (global_capabilities.users.list_all.query_with_type_normal) {
		tabmenu.add_tab('Normal', retab.bind(null, url_api_users_with_type('normal')));
	}
	tabmenu.build_and_append_to(view.content_elem);
}

function elem_timezone_marker() {
	const offset = -(new Date()).getTimezoneOffset();
	const offset_str = (() => {
		if (offset > 0) {
			return 'UTC+' + offset / 60;
		}
		if (offset < 0) {
			return 'UTC' + offset / 60;
		}
		return 'UTC';
	})();
	return elem_with_text('sup', offset_str);
}

function datetime_to_string(date) {
	const year = date.getFullYear();
	const month = date.getMonth() + 1;
	const day = date.getDate();
	const hour = date.getHours();
	const minute = date.getMinutes();
	const second = date.getSeconds();
	return String().concat(year, '-', month < 10 ? '0' : '', month, '-', day < 10 ? '0' : '', day,
		' ', hour < 10 ? '0' : '', hour, ':', minute < 10 ? '0' : '', minute, ':', second < 10 ? '0' : '', second);
}

function append_common_form_elems_for_add_problem_and_reupload_problem(form, problem = null) {
	const ignore_simfile_select = form.append_select('ignore_simfile', 'Ignore simfile');
	const name_input = form.append_input_text('name', 'Name', problem?.name ?? '', 24, {
		required: false,
		trim: true,
		placeholder: 'Take from Simfile',
	});
	const label_input = form.append_input_text('label', 'Label', problem?.label ?? '', 24, {
		required: false,
		trim: true,
		placeholder: 'Take from Simfile',
	});

	const memory_limit_input = form.append_input_text('memory_limit_in_mib', 'Memory limit', problem?.default_memory_limit ?? '', 9, {
		required: false,
		trim: true,
		placeholder: 'Take from Simfile',
	});
	memory_limit_input.parentNode.appendChild(elem_with_text('span', '[MiB]'));

	const time_limits_select = form.append_select('time_limits', 'Time limits');
	time_limits_select.append_fieldset_option('keep_if_possible', 'Take from Simfile', {}, (fieldset) => {
		const p = elem_with_text('p', 'If some test (possibly found due to "Search for new tests") does not have the limit specified, all time limits will be reset using the model solution.');
		p.style.maxWidth = '340px';
		fieldset.append(p);
	});
	time_limits_select.append_option('reset', 'Reset using model solution', {selected: true});
	time_limits_select.append_fieldset_option('fixed', 'Fixed time limit', {}, (fieldset) => {
		const time_limit_ns_input = fieldset.append_input_hidden('fixed_time_limit_in_nsec', '');
		const time_limit_input = fieldset.append_input_text('', 'Fixed time limit (for each test)', '', 9, {
			required: true,
			trim: true,
		});
		time_limit_input.addEventListener('change', (event) => {
			// * will automatically convert to float
			const nanoseconds = Math.floor(time_limit_input.value.trim() * 1e9);
			// Convert float to decimal string
			time_limit_ns_input.value = new Intl.NumberFormat('iso', {useGrouping: false}).format(nanoseconds);
		}, {passive: true});
		time_limit_input.parentNode.appendChild(elem_with_text('span', '[s]'));
	});

	ignore_simfile_select.append_option('true', 'Yes', {
		on_activation: () => {
			for (const input of [name_input, label_input, memory_limit_input]) {
				input.required = true;
				input.placeholder = '';
			}
			time_limits_select.disable_option('keep_if_possible');
		},
	});
	ignore_simfile_select.append_fieldset_option('false', 'No', {
		selected: true,
		on_activation: () => {
			for (const input of [name_input, label_input, memory_limit_input]) {
				input.required = false;
				input.placeholder = 'Take from Simfile';
			}
			time_limits_select.enable_option('keep_if_possible');
		},
	}, (fieldset) => {
		fieldset.append_checkbox('reset_scoring', 'Reset scoring', false);
		fieldset.append_checkbox('look_for_new_tests', 'Look for new tests', true);
	});

	form.append_input_file('package', 'Zipped package');
}

async function add_problem() {
	const view = new View(url_problems_add());
	const form = new Form('Add problem', url_api_problems_add());
	const type_select = form.append_select('visibility', 'Visibility');
	if (global_capabilities.problems.add_problem_with_visibility_private) {
		type_select.append_option('private', 'Private', {selected: true});
	}
	if (global_capabilities.problems.add_problem_with_visibility_contest_only) {
		type_select.append_option('contest_only', 'Contest only');
	}
	if (global_capabilities.problems.add_problem_with_visibility_public) {
		type_select.append_option('public', 'Public');
	}
	append_common_form_elems_for_add_problem_and_reupload_problem(form);
	form.append_submit_button('Add problem', {css_classes: 'blue'});
	form.success_handler = (response, {show_success}) => {
		view_job(true, response.job.id);
		show_success('Success');
	};
	form.attach_to(view.content_elem);
}

async function reupload_problem(problem_id) {
	const view = new View(url_problem_reupload(problem_id));
	const problem = await view.get_from_api(url_api_problem(problem_id));
	console.log(problem);
	const form = new Form(`Reupload problem ${problem_id}`, url_api_problem_reupload(problem_id));
	append_common_form_elems_for_add_problem_and_reupload_problem(form, problem);
	form.append_submit_button('Reupload problem', {css_classes: 'blue'});
	form.success_handler = (response, {show_success}) => {
		view_job(true, response.job.id);
		show_success('Success');
	};
	form.attach_to(view.content_elem);
}

function ProblemsLister(elem, query_url, {
	show_owner_column = true,
	show_updated_at_column = true,
}) {
	if (this === window) {
		throw new Error('Call as "new ProblemsLister()", not "ProblemsLister()"');
	}

	const self = this;
	self.process_first_api_response = (list) => {
		if (list.length === 0) {
			self.elem.parentNode.appendChild(elem_with_text('p', 'There are no problems to show...'));
			return;
		}

		const thead = document.createElement('thead');
		thead.appendChild(elem_with_text('th', 'Id'));
		thead.appendChild(elem_with_class_and_text('th', 'visibility', 'Visibility'));
		thead.appendChild(elem_with_class_and_text('th', 'label', 'Label'));
		thead.appendChild(elem_with_class_and_text('th', 'name_and_tags', 'Name and tags'));
		if (show_owner_column) {
			thead.appendChild(elem_with_class_and_text('th', 'owner', 'Owner'));
		}
		if (show_updated_at_column) {
			thead.appendChild(elem_with_class_of('th', 'updated_at', 'Updated at', elem_timezone_marker()));
		}
		thead.appendChild(elem_with_class_and_text('th', 'actions', 'Actions'));
		self.elem.appendChild(thead);
		self.tbody = document.createElement('tbody');
		self.elem.appendChild(self.tbody);

		self.process_api_response(list);
	};
	self.process_api_response = (list) => {
		self.next_query_suffix = '/id</' + list[list.length - 1].id;

		const document_fragment = document.createDocumentFragment();
		for (const problem of list) {
			const row = document.createElement('tr');
			row.appendChild(elem_with_text('td', problem.id));
			row.appendChild(elem_with_text('td', snake_case_to_user_string(problem.visibility)));
			row.appendChild(elem_with_text('td', problem.label));

			const tags_elem = elem_with_class('div', 'tags');
			row.appendChild(elem_of('td', elem_with_class_of('div', 'name_and_tags', elem_with_text('span', problem.name), tags_elem)));
			if (problem.capabilities.view_public_tags) {
				for (const tag_name of problem.tags.public) {
					tags_elem.appendChild(elem_with_text('label', tag_name));
				}
			}
			if (problem.capabilities.view_hidden_tags) {
				for (const tag_name of problem.tags.hidden) {
					tags_elem.appendChild(elem_with_class_and_text('label', 'hidden', tag_name));
				}
			}

			if (show_owner_column) {
				const td = row.appendChild(document.createElement('td'));
				if (problem.capabilities.view_owner) {
					if (problem.owner != null) {
						td.appendChild(a_view_button(url_user(problem.owner.id), problem.owner.username, undefined, view_user.bind(null, true, problem.owner.id))); // TODO: refactor it
					} else if (problem.capabilities.view_owner) {
						td.textContent = '(Deleted)';
					}
				}
			}

			if (show_updated_at_column) {
				row.appendChild(elem_with_text('td', problem.capabilities.view_update_time ? datetime_to_string(new Date(problem.updated_at)) : ''));
			}

			if (problem.capabilities.view_final_submission_full_status && problem.final_submission_full_status != null) {
				row.classList.add(...submission_status_to_css_class_list(problem.final_submission_full_status));
			}

			const td = document.createElement('td');
			td.append.apply(td, ActionsToHTML.problem(problem, false));
			row.appendChild(td);
			document_fragment.appendChild(row);
		}
		self.tbody.appendChild(document_fragment);
	};

	Lister.call(self, elem, query_url, '');
}

function list_problems() {
	const view = new View(url_problems());
	view.content_elem.appendChild(elem_with_text('h1', 'Problems'));
	if (global_capabilities.problems.add_problem) {
		view.content_elem.appendChild(elem_link_with_class_to_view('btn', 'Add problem', add_problem, url_problems_add));
	}

	const retab = (url_all_func, url_with_visibility_func, list_capabilities, extra_params, elem) => {
		const do_retab = (url, elem) => {
			const table = elem.appendChild(elem_with_class('table', 'problems stripped'));
			new ProblemsLister(table, url, extra_params);
		};

		const tabmenu = new TabMenuBuilder();
		if (list_capabilities.query_all) {
			tabmenu.add_tab('All', do_retab.bind(null, url_all_func()));
		}
		if (list_capabilities.query_with_visibility_public) {
			tabmenu.add_tab('Public', do_retab.bind(null, url_with_visibility_func('public')));
		}
		if (list_capabilities.query_with_visibility_contest_only) {
			tabmenu.add_tab('Contest only', do_retab.bind(null, url_with_visibility_func('contest_only')));
		}
		if (list_capabilities.query_with_visibility_private) {
			tabmenu.add_tab('Private', do_retab.bind(null, url_with_visibility_func('private')));
		}
		tabmenu.build_and_append_to(elem);
	};

	const can_list_something = (list_capabilities) => {
		return list_capabilities.query_all || list_capabilities.query_with_visibility_public ||
				list_capabilities.query_with_visibility_contest_only || list_capabilities.query_with_visibility_private;
	};

	const tabmenu = new TabMenuBuilder();
	if (can_list_something(global_capabilities.problems.list_all)) {
		tabmenu.add_tab('All problems', retab.bind(
			null,
			url_api_problems,
			url_api_problems_with_visibility,
			global_capabilities.problems.list_all,
			{
				show_owner_column: window.signed_user_type == 'admin' || window.signed_user_type == 'teacher',
				show_updated_at_column: window.signed_user_type == 'admin' || window.signed_user_type == 'teacher',
			}
		));
	}
	if (is_signed_in() && can_list_something(global_capabilities.problems.list_my)) {
		tabmenu.add_tab('My problems', retab.bind(
			null,
			url_api_user_problems.bind(null, signed_user_id),
			url_api_user_problems_with_visibility.bind(null, signed_user_id),
			global_capabilities.problems.list_my,
			{
				show_owner_column: false,
				show_updated_at_column: window.signed_user_type == 'admin' || window.signed_user_type == 'teacher',
			}
		));
	}
	tabmenu.build_and_append_to(view.content_elem);
}

function submission_language_to_user_string(submission_language) {
	switch (submission_language) {
		case 'c11': return 'C11';
		case 'cpp11': return 'C++11';
		case 'pascal': return 'Pascal';
		case 'cpp14': return 'C++14';
		case 'cpp17': return 'C++17';
		case 'python': return 'Python';
		case 'rust': return 'Rust';
		case 'cpp20': return 'C++20';
		default: assert(false, 'unexpected submission_language: ' + submission_language);
	}
}

function submission_status_to_css_class_list(submission_status) {
	switch (submission_status) {
		case 'ok': return ['status', 'green'];
		case 'wa': return ['status', 'red'];
		case 'tle': return ['status', 'yellow'];
		case 'mle': return ['status', 'yellow'];
		case 'ole': return ['status', 'yellow'];
		case 'rte': return ['status', 'intense-red'];
		case 'pending': return ['status'];
		case 'compilation_error': return ['status', 'purple'];
		case 'checker_compilation_error': return ['status', 'blue'];
		case 'judge_error': return ['status', 'blue'];
		default: assert(false, 'unexpected submission_status: ', submission_status);
	}
}

function submission_status_to_user_string(submission_status) {
	switch (submission_status) {
		case 'ok': return 'OK';
		case 'wa': return 'Wrong answer';
		case 'tle': return 'Time limit exceeded';
		case 'mle': return 'Memory limit exceeded';
		case 'ole': return 'Output size limit exceeded';
		case 'rte': return 'Runtime error';
		case 'pending': return 'Pending';
		case 'compilation_error': return 'Compilation error';
		case 'checker_compilation_error': return 'Checker compilation error';
		case 'judge_error': return 'Judge error';
		default: assert(false, 'unexpected submission_status: ', submission_status);
	}
}

function submission_type_to_user_string(submission, show_if_problem_final) {
	switch (submission.type) {
		case 'normal': {
			let res = '';
			if (show_if_problem_final && submission.capabilities.view_if_problem_final && submission.is_problem_final) {
				res = 'Problem';
			}
			if (submission.capabilities.view_if_contest_problem_final && submission.is_contest_problem_final) {
				if (res == '') {
					res = 'Contest';
				} else {
					res += ' + contest'
				}
			}
			if (!submission.capabilities.view_if_contest_problem_final && submission.capabilities.view_if_contest_problem_initial_final && submission.is_contest_problem_initial_final) {
				if (res == '') {
					res = 'Contest initial';
				} else {
					res += ' + contest initial';
				}
			}
			if (res == '') {
				return 'Normal';
			} else {
				res += ' final';
				return res;
			}
		}
		case 'ignored': return 'Ignored';
		case 'problem_solution': return 'Problem solution';
	}
}

function SubmissionsLister(elem, query_url, {
	show_user_column = true,
	show_if_problem_final = true,
}) {
	if (this === window) {
		throw new Error('Call as "new SubmissionsLister()", not "SubmissionsLister()"');
	}

	const self = this;
	self.process_first_api_response = (list) => {
		if (list.length === 0) {
			self.elem.parentNode.appendChild(elem_with_text('p', 'There are no submissions to show...'));
			return;
		}

		const thead = document.createElement('thead');
		thead.appendChild(elem_with_text('th', 'Id'));
		thead.appendChild(elem_with_class_and_text('th', 'language', 'Lang'));
		if (show_user_column) {
			thead.appendChild(elem_with_class_and_text('th', 'user', 'User'));
		}
		thead.appendChild(elem_with_class_of('th', 'created_at', 'Added', elem_timezone_marker()));
		thead.appendChild(elem_with_class_and_text('th', 'problem', 'Problem'));
		thead.appendChild(elem_with_class_and_text('th', 'status', 'Status'));
		thead.appendChild(elem_with_class_and_text('th', 'score', 'Score'));
		thead.appendChild(elem_with_class_and_text('th', 'type', 'Type'));
		thead.appendChild(elem_with_class_and_text('th', 'actions', 'Actions'));
		self.elem.appendChild(thead);
		self.tbody = document.createElement('tbody');
		self.elem.appendChild(self.tbody);

		self.process_api_response(list);
	};
	self.process_api_response = (list) => {
		self.next_query_suffix = '/id</' + list[list.length - 1].id;

		const document_fragment = document.createDocumentFragment();
		for (const submission of list) {
			const row = document.createElement('tr');
			if (submission.type == 'ignored') {
				row.classList.add('ignored');
			}
			row.appendChild(elem_with_text('td', submission.id));
			row.appendChild(elem_with_text('td', submission_language_to_user_string(submission.language)));

			if (show_user_column) {
				const td = row.appendChild(elem_with_class('td', 'user'));
				if (submission.user != null) {
					td.appendChild(a_view_button(url_user(submission.user.id), submission.user.first_name + ' ' + submission.user.last_name, undefined, view_user.bind(null, true, submission.user.id))); // TODO: refactor it
				} else {
					td.textContent = 'System';
				}
			}

			row.appendChild(elem_of('td', a_view_button(url_submission(submission.id), datetime_to_string(new Date(submission.created_at)), undefined, view_submission.bind(null, true, submission.id)))); // TODO: refactor it

			if (submission.contest_problem != null) {
				row.appendChild(elem_of('td',
					a_view_button(url_contest(submission.contest.id), submission.contest.name, undefined, view_contest.bind(null, true, submission.contest.id)), // TODO: refactor it
					' / ',
					a_view_button(url_contest_round(submission.contest_round.id), submission.contest_round.name, undefined, view_contest_round.bind(null, true, submission.contest_round.id)), // TODO: refactor it
					' / ',
					a_view_button(url_contest_problem(submission.contest_problem.id), submission.contest_problem.name, undefined, view_contest_problem.bind(null, true, submission.contest_problem.id)), // TODO: refactor it
				));
			} else {
				row.appendChild(elem_of('td', a_view_button(url_problem(submission.problem.id), submission.problem.name, undefined, view_problem.bind(null, true, submission.problem.id)))); // TODO: refactor it
			}

			const status_td = row.appendChild(document.createElement('td'));
			if (submission.capabilities.view_full_status) {
				status_td.textContent = submission_status_to_user_string(submission.full_status);
				status_td.classList.add(...submission_status_to_css_class_list(submission.full_status));
			} else {
				status_td.textContent = 'Initial: ' + submission_status_to_user_string(submission.initial_status);
				status_td.classList.add(...submission_status_to_css_class_list(submission.initial_status));
			}

			const score_td = row.appendChild(document.createElement('td'));
			if (submission.capabilities.view_score) {
				score_td.textContent = submission.score;
			}

			row.appendChild(elem_with_text('td', submission_type_to_user_string(submission, show_if_problem_final)));
			row.appendChild(elem_of('td', ...ActionsToHTML.submission(submission)));
			document_fragment.appendChild(row);
		}
		self.tbody.appendChild(document_fragment);
	};

	Lister.call(self, elem, query_url, '');
}

function append_tabbed_submissions_lister(url_all_func, url_with_type_func, list_capabilities, extra_params, elem) {
	const retab = (url, elem) => {
		const table = elem.appendChild(elem_with_class('table', 'submissions'));
		new SubmissionsLister(table, url, extra_params);
	};

	const tabmenu = new TabMenuBuilder();
	if (list_capabilities.query_all) {
		tabmenu.add_tab('All', retab.bind(null, url_all_func()));
	}
	if (list_capabilities.query_with_type_final) {
		tabmenu.add_tab('Final', retab.bind(null, url_with_type_func('final')));
	}
	if (list_capabilities.query_with_type_problem_final) {
		tabmenu.add_tab('Problem final', retab.bind(null, url_with_type_func('problem_final')));
	}
	if (list_capabilities.query_with_type_contest_problem_final) {
		tabmenu.add_tab('Contest final', retab.bind(null, url_with_type_func('contest_problem_final')));
	}
	if (list_capabilities.query_with_type_ignored) {
		tabmenu.add_tab('Ignored', retab.bind(null, url_with_type_func('ignored')));
	}
	if (list_capabilities.query_with_type_problem_solution) {
		tabmenu.add_tab('Problem solutions', retab.bind(null, url_with_type_func('problem_solution')));
	}
	tabmenu.build_and_append_to(elem);
}

function can_list_any_submissions(list_capabilities) {
	return list_capabilities.query_all || list_capabilities.query_with_type_final ||
	       list_capabilities.query_with_type_problem_final ||
	       list_capabilities.query_with_type_contest_problem_final ||
	       list_capabilities.query_with_type_ignored ||
	       list_capabilities.query_with_type_problem_solution;
}

function list_submissions() {
	const view = new View(url_submissions());
	view.content_elem.appendChild(elem_with_text('h1', 'Submissions'));

	const tabmenu = new TabMenuBuilder();
	if (can_list_any_submissions(global_capabilities.submissions.list_all)) {
		tabmenu.add_tab('All submissions', append_tabbed_submissions_lister.bind(
			null,
			url_api_submissions,
			url_api_submissions_with_type,
			global_capabilities.submissions.list_all,
			{}
		));
	}
	if (is_signed_in() && can_list_any_submissions(global_capabilities.submissions.list_my)) {
		tabmenu.add_tab('My submissions', append_tabbed_submissions_lister.bind(
			null,
			url_api_user_submissions.bind(null, signed_user_id),
			url_api_user_submissions_with_type.bind(null, signed_user_id),
			global_capabilities.submissions.list_my,
			{
				show_user_column: false,
			}
		));
	}
	tabmenu.build_and_append_to(view.content_elem);
}

function job_type_to_user_string(job_type) {
	switch (job_type) {
		case 'judge_submission': return 'Judge submission';
		case 'add_problem': return 'Add problem';
		case 'reupload_problem': return 'Reupload problem';
		case 'edit_problem': return 'Edit problem';
		case 'delete_problem': return 'Delete problem';
		case 'reselect_final_submissions_in_contest_problem': return 'Reselect final submissions in contest problem';
		case 'delete_user': return 'Delete user';
		case 'delete_contest': return 'Delete contest';
		case 'delete_contest_round': return 'Delete contest round';
		case 'delete_contest_problem': return 'Delete contest problem';
		case 'reset_problem_time_limits_using_model_solution': return 'Reset problem time limits using model solution';
		case 'merge_problems': return 'Merge problems';
		case 'rejudge_submission': return 'Rejudge submission';
		case 'delete_internal_file': return 'Delete internal file';
		case 'change_problem_statement': return 'Change problem statement';
		case 'merge_users': return 'Merge users';
		default: assert(false, 'unexpected job_type: ' + job_type);
	}
}

function job_status_to_css_class_list(job_status) {
	switch (job_status) {
		case 'pending': return ['status'];
		case 'in_progress': return ['status', 'yellow'];
		case 'done': return ['status', 'green'];
		case 'failed': return ['status', 'red'];
		case 'cancelled': return ['status', 'blue'];
		default: assert(false, 'unexpected job_status: ', job_status);
	}
}

function job_status_to_user_string(job_status) {
	switch (job_status) {
		case 'pending': return 'Pending';
		case 'in_progress': return 'In progress';
		case 'done': return 'Done';
		case 'failed': return 'Failed';
		case 'cancelled': return 'Cancelled';
		default: assert(false, 'unexpected job_status: ', job_status);
	}
}
function JobsLister(elem, query_url, {
	show_creator_column = true,
}) {
	if (this === window) {
		throw new Error('Call as "new JobsLister()", not "JobsLister()"');
	}

	const self = this;
	self.process_first_api_response = (list) => {
		if (list.length === 0) {
			self.elem.parentNode.appendChild(elem_with_text('p', 'There are no jobs to show...'));
			return;
		}

		const thead = document.createElement('thead');
		thead.appendChild(elem_with_text('th', 'Id'));
		thead.appendChild(elem_with_class_and_text('th', 'type', 'Type'));
		thead.appendChild(elem_with_class_and_text('th', 'priority', 'Priority'));
		thead.appendChild(elem_with_class_of('th', 'created_at', 'Created at', elem_timezone_marker()));
		thead.appendChild(elem_with_class_and_text('th', 'status', 'Status'));
		if (show_creator_column) {
			thead.appendChild(elem_with_class_and_text('th', 'creator', 'Creator'));
		}
		thead.appendChild(elem_with_class_and_text('th', 'info', 'Info'));
		thead.appendChild(elem_with_class_and_text('th', 'actions', 'Actions'));
		self.elem.appendChild(thead);
		self.tbody = document.createElement('tbody');
		self.elem.appendChild(self.tbody);

		self.process_api_response(list);
	};
	self.process_api_response = (list) => {
		self.next_query_suffix = '/id</' + list[list.length - 1].id;

		const document_fragment = document.createDocumentFragment();
		for (const job of list) {
			const row = document.createElement('tr');
			row.appendChild(elem_with_text('td', job.id));
			row.appendChild(elem_with_text('td', job_type_to_user_string(job.type)));
			row.appendChild(elem_with_text('td', job.priority));
			row.appendChild(elem_of('td', a_view_button(url_job(job.id), datetime_to_string(new Date(job.created_at)), undefined, view_job.bind(null, true, job.id)))); // TODO: refactor it

			const status_td = row.appendChild(document.createElement('td'));
			status_td.textContent = job_status_to_user_string(job.status);
			status_td.classList.add(...job_status_to_css_class_list(job.status));

			if (show_creator_column) {
				const td = row.appendChild(elem_with_class('td', 'creator'));
				if (job.creator != null) {
					if (job.creator.username != null) {
						td.appendChild(a_view_button(url_user(job.creator.id), job.creator.username, undefined, view_user.bind(null, true, job.creator.id))); // TODO: refactor it
					} else {
						td.textContent = `deleted (id: ${job.creator.id})`;
					}
				} else {
					td.textContent = 'System';
				}
			}

			const info_div = elem_with_class('div', 'info');
			switch (job.type) {
			case 'judge_submission':
			case 'rejudge_submission':
				info_div.appendChild(elem_with_text('label', 'submission'));
				info_div.appendChild(a_view_button(url_submission(job.submission_id), job.submission_id, undefined, view_submission.bind(null, true, job.submission_id))); // TODO: refactor it
				break;
			case 'add_problem':
			case 'reupload_problem':
			case 'edit_problem':
			case 'delete_problem':
			case 'change_problem_statement':
			case 'reset_problem_time_limits_using_model_solution':
				info_div.appendChild(elem_with_text('label', 'problem'));
				info_div.appendChild(a_view_button(url_submission(job.problem_id), job.problem_id, undefined, view_problem.bind(null, true, job.problem_id))); // TODO: refactor it
				break;
			case 'reselect_final_submissions_in_contest_problem':
			case 'delete_contest_problem':
				info_div.appendChild(elem_with_text('label', 'contest problem'));
				info_div.appendChild(a_view_button(url_submission(job.contest_problem_id), job.contest_problem_id, undefined, view_contest_problem.bind(null, true, job.contest_problem_id))); // TODO: refactor it
				break;
			case 'delete_user':
				info_div.appendChild(elem_with_text('label', 'user'));
				info_div.appendChild(a_view_button(url_submission(job.user_id), job.user_id, undefined, view_user.bind(null, true, job.user_id))); // TODO: refactor it
				break;
			case 'delete_contest':
				info_div.appendChild(elem_with_text('label', 'contest'));
				info_div.appendChild(a_view_button(url_submission(job.contest_id), job.contest_id, undefined, view_contest.bind(null, true, job.contest_id))); // TODO: refactor it
				break;
			case 'delete_contest_round':
				info_div.appendChild(elem_with_text('label', 'contest round'));
				info_div.appendChild(a_view_button(url_submission(job.contest_round_id), job.contest_round_id, undefined, view_contest_round.bind(null, true, job.contest_round_id))); // TODO: refactor it
				break;
			case 'merge_problems':
				info_div.appendChild(elem_with_text('label', 'donor problem'));
				info_div.appendChild(a_view_button(url_submission(job.donor_problem_id), job.donor_problem_id, undefined, view_problem.bind(null, true, job.donor_problem_id))); // TODO: refactor it
				info_div.appendChild(elem_with_text('label', 'target problem'));
				info_div.appendChild(a_view_button(url_submission(job.target_problem_id), job.target_problem_id, undefined, view_problem.bind(null, true, job.target_problem_id))); // TODO: refactor it
				break;
			case 'delete_internal_file':
				info_div.append(elem_with_text('label', 'internal file'), job.internal_file_id);
				break;
			case 'merge_users':
				info_div.appendChild(elem_with_text('label', 'donor user'));
				info_div.appendChild(a_view_button(url_submission(job.donor_user_id), job.donor_user_id, undefined, view_user.bind(null, true, job.donor_user_id))); // TODO: refactor it
				info_div.appendChild(elem_with_text('label', 'target user'));
				info_div.appendChild(a_view_button(url_submission(job.target_user_id), job.target_user_id, undefined, view_user.bind(null, true, job.target_user_id))); // TODO: refactor it
				break;
			default: assert(false, 'unexpected job.type: ' + job.type);
			}
			row.appendChild(elem_of('td', info_div));

			row.appendChild(elem_of('td', ...ActionsToHTML.job(job)));
			document_fragment.appendChild(row);
		}
		self.tbody.appendChild(document_fragment);
	};

	Lister.call(self, elem, query_url, '');
}

function append_tabbed_jobs_lister(url_all_func, url_with_status_func, list_capabilities, extra_params, elem) {
	const retab = (url, elem) => {
		const table = elem.appendChild(elem_with_class('table', 'jobs'));
		new JobsLister(table, url, extra_params);
	};

	const tabmenu = new TabMenuBuilder();
	if (list_capabilities.query_all) {
		tabmenu.add_tab('All', retab.bind(null, url_all_func()));
	}
	if (list_capabilities.query_with_status_pending) {
		tabmenu.add_tab('Pending', retab.bind(null, url_with_status_func('pending')));
	}
	if (list_capabilities.query_with_status_in_progress) {
		tabmenu.add_tab('In progress', retab.bind(null, url_with_status_func('in_progress')));
	}
	if (list_capabilities.query_with_status_done) {
		tabmenu.add_tab('Done', retab.bind(null, url_with_status_func('done')));
	}
	if (list_capabilities.query_with_status_failed) {
		tabmenu.add_tab('Failed', retab.bind(null, url_with_status_func('failed')));
	}
	if (list_capabilities.query_with_status_cancelled) {
		tabmenu.add_tab('Cancelled', retab.bind(null, url_with_status_func('cancelled')));
	}
	tabmenu.build_and_append_to(elem);
}

function can_list_any_jobs(list_capabilities) {
	return list_capabilities.query_all || list_capabilities.query_with_status_pending ||
	       list_capabilities.query_with_status_in_progress ||
	       list_capabilities.query_with_status_done ||
	       list_capabilities.query_with_status_failed ||
	       list_capabilities.query_with_status_cancelled;
}

function list_jobs() {
	const view = new View(url_jobs());
	view.content_elem.appendChild(elem_with_text('h1', 'Jobs'));

	const tabmenu = new TabMenuBuilder();
	if (can_list_any_jobs(global_capabilities.jobs.list_all)) {
		tabmenu.add_tab('All jobs', append_tabbed_jobs_lister.bind(
			null,
			url_api_jobs,
			url_api_jobs_with_status,
			global_capabilities.jobs.list_all,
			{}
		));
	}
	if (is_signed_in() && can_list_any_jobs(global_capabilities.jobs.list_my)) {
		tabmenu.add_tab('My jobs', append_tabbed_jobs_lister.bind(
			null,
			url_api_user_jobs.bind(null, signed_user_id),
			url_api_user_jobs_with_status.bind(null, signed_user_id),
			global_capabilities.jobs.list_my,
			{
				show_creator_column: false,
			}
		));
	}
	tabmenu.build_and_append_to(view.content_elem);
}

////////////////////////// The above code has gone through refactor //////////////////////////

function text_to_safe_html(str) { // This is deprecated because DOM elements have innerText property (see elem_with_text() function)
	const x = document.createElement('span');
	x.innerText = str;
	return x.innerHTML;
}
function signed_user_is_admin() { // This is deprecated
	return signed_user_type == 'admin';
}
function signed_user_is_teacher() { // This is deprecated
	return signed_user_type == 'teacher';
}
function signed_user_is_teacher_or_admin() { // This is deprecated
	return signed_user_is_teacher() || signed_user_is_admin();
}
// Returns value of cookie @p name or empty string
function get_cookie(name) {
	name = name + '=';
	var arr = document.cookie.split(';');
	for (var i = 0; i < arr.length; ++i) {
		var j = 0;
		while (arr[i].charAt(j) == ' ')
			++j;
		if (arr[i].indexOf(name, j) == j)
			return arr[i].substring(j + name.length, arr[i].length);
	}

	return '';
}

function StaticMap() {
	var this_ = this;
	this.data = [];

	this.cmp = function(a, b) { return a[0] - b[0]; };

	this.add = function(key, val) {
		this_.data.push([key, val]);
	};

	this.prepare = function() { this_.data.sort(this_.cmp); };

	this.get = function(key) {
		if (this_.data.length === 0)
			return null;

		var l = 0, r = this_.data.length - 1;
		while (l < r) {
			var m = (l + r) >> 1;
			if (this_.data[m][0] < key)
				l = m + 1;
			else
				r = m;
		}

		return (this_.data[l][0] == key ? this_.data[l][1] : null);
	};
}

/* ============================ URL hash parser ============================ */
var old_url_hash_parser = {};
(function () {
	var args = window.location.hash; // Must begin with '#'
	var beg = 0; // Points to the '#' just before the next argument

	old_url_hash_parser.next_arg  = function() {
		var pos = args.indexOf('#', beg + 1);
		if (pos === -1)
			return args.substring(beg + 1);

		return args.substring(beg + 1, pos);
	};

	old_url_hash_parser.extract_next_arg  = function() {
		var pos = args.indexOf('#', beg + 1), res;
		if (pos === -1) {
			if (beg >= args.length)
				return '';

			res = args.substring(beg + 1);
			beg = args.length;
			return res;
		}

		res = args.substring(beg + 1, pos);
		beg = pos;
		return res;
	};

	old_url_hash_parser.empty = function() { return (beg >= args.length); };

	old_url_hash_parser.assign = function(new_hash) {
		beg = 0;
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
	};

	old_url_hash_parser.assign_as_parsed = function(new_hash) {
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
		beg = args.length;
	};


	old_url_hash_parser.append = function(next_args) {
		if (next_args.charAt(0) !== '#')
			args += '#' + next_args;
		else
			args += next_args;
	};

	old_url_hash_parser.parsed_prefix = function() { return args.substring(0, beg); };
}).call();


///////////////////////////////// Below code requires jQuery /////////////////////////////////
function tz_marker() {
	var tzo = -(new Date()).getTimezoneOffset();
	return String().concat('<sup>UTC', (tzo >= 0 ? '+' : ''), tzo / 60,
		'</sup>');
}
function add_tz_marker(elem) {
	elem = $(elem);
	elem.children('sup').remove();
	elem.append(tz_marker);
}
function date_to_datetime_str(date, trim_zero_seconds /*= false*/) {
	var month = date.getMonth() + 1;
	var day = date.getDate();
	var hours = date.getHours();
	var minutes = date.getMinutes();
	var seconds = date.getSeconds();
	month = (month < 10 ? '0' : '') + month;
	day = (day < 10 ? '0' : '') + day;
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;

	if (trim_zero_seconds && seconds === 0)
		return String().concat(date.getFullYear(), '-', month, '-', day, ' ', hours, ':', minutes);

	seconds = (seconds < 10 ? '0' : '') + seconds;
	return String().concat(date.getFullYear(), '-', month, '-', day, ' ', hours, ':', minutes, ':', seconds);
}
function utcdt_or_tm_to_Date(time) {
	if (isNaN(time)) {
		// Convert infinities to infinities so they are comparable with other Dates
		if (time === '-inf')
			return -Infinity;
		if (time == '+inf')
			return Infinity;

		var args = time.split(/[- :]/);
		--args[1]; // fit month in [0, 11]
		return new Date(Date.UTC.apply(this, args));
	}

	return new Date(time * 1000);
}
// Converts datetimes to local
function normalize_datetime(elem, add_tz /*= false*/, trim_zero_seconds /*= false*/) {
	elem = $(elem);
	if (elem.attr('datetime') === undefined)
		return elem;

	// Add the timezone part
	elem.text(date_to_datetime_str(utcdt_or_tm_to_Date(elem.attr('datetime')), trim_zero_seconds));
	if (add_tz)
		elem.append(tz_marker());

	return elem;
}
function infdatetime_to(elem, infdt, neg_inf_text, inf_text) {
	if (infdt[0] == '-')
		return elem.text(neg_inf_text);
	else if (infdt[0] == '+')
		return elem.text(inf_text);
	else
		return normalize_datetime(elem.text(infdt)
			.attr('datetime', infdt));
}
// Produces a span that will update every second and show remaining time
function countdown_clock(target_date) {
	var span = $('<span>');
	var update_countdown = function() {
		var diff = target_date - current_server_time();
		if (!$.contains(document.documentElement, span[0]))
			return;

		if (diff < 0)
			diff = 0;
		else
			setTimeout(update_countdown, (diff + 999) % 1000 + 1);

		diff = Math.round(diff / 1000); // To mitigate little latency that may occur
		var diff_text = '';
		var make_diff_text = function(n, singular, plural) {
			return n + (n == 1 ? singular : plural);
		};

		if (diff >= 3600)
			diff_text += make_diff_text(Math.floor(diff / 3600), ' hour ', ' hours ');
		if (diff >= 60)
			diff_text += make_diff_text(Math.floor((diff % 3600) / 60), ' minute and ', ' minutes and ');
		diff_text += make_diff_text(diff % 60, ' second', ' seconds');
		span.text(diff_text);
	}

	setTimeout(update_countdown);
	return span;
}

// Dropdowns
$(document).ready(function(){
	$(document).click(function(event) {
		if ($(event.target).parent('.dropmenu .dropmenu-toggle') !== 0) {
			var dropmenu = $(event.target).closest('.dropmenu');
			if (dropmenu.is('.open'))
				dropmenu.removeClass('open');
			else {
				$('.dropmenu.open').removeClass('open');
				dropmenu.addClass('open');
			}
		} else
			$('.dropmenu.open').removeClass('open');
	});
});
function append_btn_tooltip(elem, text) {
	var tooltip = $('<span>', {
		class: 'btn-tooltip',
		text: text,
		click: function(e) {
			e.stopPropagation();
			tooltip.remove();
		}
	});
	tooltip.appendTo(elem);
	setTimeout(function() {
		tooltip.fadeOut('slow', function() {
			tooltip.remove()
		});
	}, 750);
}
function copy_to_clipboard_btn(small, btn_text, get_text_to_copy) {
	if (!document.queryCommandSupported('copy'))
		return $();

	return $('<a>', {
		class: small ? 'btn-small' : 'btn',
		style: 'position: relative',
		text: btn_text,
		click: function() {
			copy_to_clipboard(get_text_to_copy).then(() => {
				append_btn_tooltip($(this), 'Copied to clipboard!');
			});
		}
	});
}
// Handle navbar correct size
function normalize_navbar() {
	var navbar = $('.navbar');
	navbar.css('position', 'fixed');

	if (navbar.outerWidth() > $(window).width())
		navbar.css('position', 'absolute');
}
$(document).ready(normalize_navbar);
$(window).resize(normalize_navbar);

// Handle menu correct size
function normalize_menu() {
	var menu = $('.menu');
	menu.css('height', 'auto');

	if (menu.outerHeight() <= $(window).height()) {
		menu.css('height', '100%');
		menu.css('position', 'fixed');
	} else {
		menu.css('position', 'absolute');
		menu.outerHeight($(document).height());
	}
}
$(document).ready(normalize_menu);
$(window).resize(normalize_menu);

// Adding csrf token to a form; This is deprecated
function add_csrf_token_to(form) {
	var x = $(form);
	x.find('input[name="csrf_token"]').remove(); // Avoid duplication
	x.append('<input type="hidden" name="csrf_token" value="' +
		get_cookie('csrf_token') + '">');
	return x;
}

// Adding csrf token just before submitting a form
$(document).ready(function() {
	$('form').submit(function() { add_csrf_token_to(this); });
});

/* const */ var fade_in_duration = 50; // milliseconds

// For best effect, the values below should be the same
/* const */ var oldloader_show_delay = 400; // milliseconds
/* const */ var timed_hide_delay = 400; // milliseconds

/* ================================= Oldloader ================================= */
function remove_oldloader(elem) {
	elem.removeChild(elem.querySelector('.oldloader'));
}
function try_remove_oldloader(elem) {
	var child = elem.querySelector('.oldloader');
	if (child != null) {
		elem.removeChild(child);
	}
}
function try_remove_oldloader_info(elem) {
	var child = elem.querySelector('.oldloader-info');
	if (child != null) {
		elem.removeChild(child);
	}
}
function append_oldloader(elem) {
	try_remove_oldloader_info(elem);
	var oldloader;
	if (elem.style.animationName === undefined && elem.style.WebkitAnimationName == undefined) {
		oldloader = elem_with_class('img', 'oldloader');
		oldloader.setAttribute('src', '/kit/img/oldloader.gif');
	} else {
		oldloader = elem_with_class('span', 'oldloader');
		oldloader.style.display = 'none';
		oldloader.appendChild(elem_with_class('div', 'spinner'));
		setTimeout(() => {
			$(oldloader).fadeIn(fade_in_duration);
		}, oldloader_show_delay);
	}
	elem.appendChild(oldloader);
}
function show_success_via_oldloader(elem, html) {
	try_remove_oldloader(elem);
	var oldloader_info = elem_with_class('span', 'oldloader-info success');
	oldloader_info.style.display = 'none';
	oldloader_info.innerHTML = html;
	$(oldloader_info).fadeIn(fade_in_duration);
	elem.appendChild(oldloader_info);
	timed_hide_show(elem.closest('.oldmodal'));
}
function show_error_via_oldloader(elem, response, err_status, try_again_handler) {
	if (err_status == 'success' || err_status == 'error' || err_status === undefined)
		err_status = '';
	else
		err_status = '; ' + err_status;

	elem = $(elem);
	try_remove_oldloader(elem[0]);
	elem.append($('<span>', {
		class: 'oldloader-info error',
		style: 'display:none',
		html: $('<span>', {
			text: "Error: " + response.status + ' ' + response.statusText + err_status
		}).add(try_again_handler === undefined ? '' : $('<center>', {
			html: $('<a>', {
				text: 'Try again',
				click: try_again_handler
			})
		}))
	}).fadeIn(fade_in_duration));

	timed_hide_show(elem.closest('.oldmodal'));

	// Additional message
	var x = elem.find('.oldloader-info > span');
	try {
		var msg = $($.parseHTML(response.responseText)).text();

		if (msg != '')
			x.text(x.text().concat("\n", msg));

	} catch (err) {
		if (response.responseText != '' && // There is a message
			response.responseText.lastIndexOf('<!DOCTYPE html>', 0) !== 0 && // Message is not a whole HTML page
			response.responseText.lastIndexOf('<!doctype html>', 0) !== 0) // Message is not a whole HTML page
		{
			x.text(x.text().concat("\n", response.responseText));
		}
	}
}

var get_unique_id = function() {
	var id = 0;
	return function() {
		return id++;
	};
}();

/* ================================= OldForm ================================= */
var OldForm = {}; // This became deprecated, use Form
OldForm.field_group = function(label_text_or_html_content, input_context_or_html_elem) {
	var input;
	if (input_context_or_html_elem instanceof jQuery)
		input = input_context_or_html_elem;
	else if (input_context_or_html_elem instanceof HTMLElement)
		input = $(input_context_or_html_elem);
	else
		input = $('<input>', input_context_or_html_elem);

	var id;
	if (input.is('input[type="checkbox"]')) {
		id = 'checkbox' + get_unique_id();
		input.attr('id', id);
	}

	var is_label_html_content = (Array.isArray(label_text_or_html_content) || label_text_or_html_content instanceof jQuery || input_context_or_html_elem instanceof HTMLElement);

	return $('<div>', {
		class: 'field-group',
		html: [
			$('<label>', {
				text: (is_label_html_content ? undefined : label_text_or_html_content),
				html: (is_label_html_content ? label_text_or_html_content : undefined),
				for: id
			}),
			input
		]
	});
};
// This became deprecated, use Form
OldForm.send_via_ajax = function(form, url, success_msg_or_handler /*= 'Success'*/, oldloader_parent)
{
	if (success_msg_or_handler === undefined)
		success_msg_or_handler = 'Success';
	if (oldloader_parent === undefined)
		oldloader_parent = $(form);

	form = $(form);
	add_csrf_token_to(form);
	append_oldloader(oldloader_parent[0]);

	// Transform data before sending
	var form_data = new FormData(form[0]);
	form.find('[trim_before_send="true"]').each(function() {
		if (this.name != null) {
			form_data.set(this.name, form_data.get(this.name).trim());
		}
	});
	// Prepare and send form
	$.ajax({
		url: url,
		type: 'POST',
		processData: false,
		contentType: false,
		data: form_data,
		headers: {'x-csrf-token': get_cookie('csrf_token')},
		success: function(resp) {
			if (typeof success_msg_or_handler === "function") {
				success_msg_or_handler.call(form, resp, oldloader_parent);
			} else
				show_success_via_oldloader(oldloader_parent[0], success_msg_or_handler);
		},
		error: function(resp, status) {
			show_error_via_oldloader(oldloader_parent, resp, status);
		}
	});
	return false;
};
// This became deprecated, use Form
function ajax_form(title, target, html, success_msg_or_handler, classes) {
	return $('<div>', {
		class: 'form-container' + (classes === undefined ? '' : ' ' + classes),
		html: $('<h1>', {text: title})
		.add('<form>', {
			method: 'post',
			html: html
		}).submit(function() {
			return OldForm.send_via_ajax(this, target, success_msg_or_handler);
		})
	});
}


/* ================================= OldModals ================================= */
function remove_oldmodals(oldmodal) {
	$(oldmodal).remove();
}
function close_oldmodal(oldmodal) {
	oldmodal = $(oldmodal);
	// Run pre-close callbacks
	oldmodal.each(function() {
		if (this.onoldmodalclose !== undefined && this.onoldmodalclose() === false)
			return;

		if ($(this).is('.oldview'))
			window.history.back();
		else
			remove_oldmodals(this);
	});
}
$(document).click(function(event) {
	if ($(event.target).is('.oldmodal'))
		close_oldmodal($(event.target));
	else if ($(event.target).is('.oldmodal .close'))
		close_oldmodal($(event.target).parent().parent());
});
function oldmodal(oldmodal_body, before_callback /*= undefined*/) {
	var mod = $('<div>', {
		class: 'oldmodal',
		style: 'display: none',
		html: $('<div>', {
			class: 'window',
			html: $('<span>', { class: 'close' }).add(oldmodal_body)
		})
	});

	if (before_callback !== undefined)
		before_callback(mod);

	mod.appendTo($(History.get_current_view_elem())).fadeIn(fade_in_duration);
	centerize_oldmodal(mod);
	return mod;
}
function centerize_oldmodal(oldmodal, allow_lowering /*= true*/) {
	var m = $(oldmodal);
	if (m.length === 0)
		return;

	if (allow_lowering === undefined)
		allow_lowering = true;

	var oldmodal_css = m.css(['display', 'position', 'left']);
	if (oldmodal_css.display === 'none') {
		// Make visible for a while to get sane width properties
		m.css({
			display: 'block',
			position: 'fixed',
			left: '200%'
		});
	}

	// Update padding-top
	var new_padding = (m.innerHeight() - m.children('div').innerHeight()) / 2;
	if (oldmodal_css.display === 'none')
		m.css(oldmodal_css);

	if (!allow_lowering) {
		var old_padding = m.css('padding-top');
		if (old_padding !== undefined && parseInt(old_padding.slice(0, -2)) < new_padding)
			return;
	}

	m.css({'padding-top': Math.max(new_padding, 0)});
}
/// Sends ajax form and shows oldmodal with the result
function oldmodal_request(title, form, target_url, success_msg) {
	var elem = oldmodal($('<h2>', {text: title}));
	OldForm.send_via_ajax(form, target_url, success_msg, elem.children());
}
function dialogue_oldmodal_request(title, info_html, go_text, go_classes, target_url, success_msg, cancel_text, remove_buttons_on_click) {
	oldmodal($('<h2>', {text: title})
		.add(info_html).add('<div>', {
			style: 'margin: 12px auto 0',
			html: $('<a>', {
				class: go_classes,
				text: go_text,
				click: function() {
					var oldloader_parent = $(this).parent().parent();
					var form = oldloader_parent.find('form');
					if (form.length === 0)
						form = $('<form>');

					if (remove_buttons_on_click)
						$(this).parent().remove();

					OldForm.send_via_ajax(form, target_url, success_msg,
						oldloader_parent);
				}
			}).add((cancel_text === undefined ? '' : $('<a>', {
				class: 'btn-small',
				text: cancel_text,
				click: function() {
					$(this).parent().parent().parent().remove();
				}
			})))
		})
	);
}

function parse_api_resp(data) {
	var names = data[0];
	if (names === undefined || (names.columns === undefined && names.fields === undefined))
		return data;

	function transform(names, data) {
		var to_obj = function(names, array) {
			var obj = {};
			var e = Math.min(names.length, array.length);
			for (var j = 0; j < e; ++j) {
				if (names[j].name === undefined)
					obj[names[j]] = array[j];
				else
					obj[names[j].name] = transform(names[j], array[j]);
			}

			return obj;
		};

		if (names.columns !== undefined) {
			var res = [];
			for (var i = 0; i < data.length; ++i)
				res.push(to_obj(names.columns, data[i]));
			return res;
		}

		// names.fields
		return to_obj(names.fields, data);
	}

	return transform(names, data.slice(1));
}

function old_API_call(ajax_url, success_handler, oldloader_parent) {
	var self = this;
	append_oldloader(oldloader_parent[0]);
	$.ajax({
		url: ajax_url,
		type: 'POST',
		processData: false,
		contentType: false,
		data: new FormData(add_csrf_token_to($('<form>')).get(0)),
		dataType: 'json',
		success: function(data, status, jqXHR) {
			remove_oldloader(oldloader_parent[0]);
			success_handler.call(this, parse_api_resp(data), status, jqXHR);
		},
		error: function(resp, status) {
			show_error_via_oldloader(oldloader_parent, resp, status,
				setTimeout.bind(null, old_API_call.bind(self, ajax_url, success_handler, oldloader_parent))); // Avoid recursion
		}
	});
}

function API_get(url, success_handler, oldloader_parent) {
	var self = this;
	append_oldloader(oldloader_parent[0]);
	$.ajax({
		url: url,
		type: 'GET',
		dataType: 'json',
		success: function(data, status, jqXHR) {
			remove_oldloader(oldloader_parent[0]);
			success_handler.call(this, data, status, jqXHR);
		},
		error: function(resp, status) {
			show_error_via_oldloader(oldloader_parent, resp, status,
				setTimeout.bind(null, API_get.bind(self, url, success_handler, oldloader_parent))); // Avoid recursion
		}
	});
}

/* ================================ Tab menu ================================ */
function tabname_to_hash(tabname) {
	return tabname.toLowerCase().replace(/ /g, '_');
}
function default_tabmenu_attacher(x) {
	x.on('tabmenuTabHasChanged', function() { x.nextAll().remove(); });
	x.appendTo(this);
}
function tabmenu_attacher_with_change_callback(tabmenu_changed_callback, x) {
	x.on('tabmenuTabHasChanged', tabmenu_changed_callback);
	default_tabmenu_attacher.call(this, x);
}
/// Triggers 'tabmenuTabHasChanged' event on the result every time an active tab is changed
function old_tabmenu(attacher, tabs) {
	var res = $('<div>', {class: 'old_tabmenu'});
	/*const*/ var prior_hash = old_url_hash_parser.parsed_prefix();

	var set_min_width = function(elem) {
		var tabm = $(elem).parent();
		var mdiv = tabm.closest('.oldmodal > div');
		if (mdiv.length === 0)
			return; // this is not in the oldmodal

		var oldmodal_css = mdiv.parent().css(['display', 'position', 'left']);
		if (oldmodal_css.display === 'none') {
			// Make visible for a while to get sane width properties
			mdiv.parent().css({
				display: 'block',
				position: 'fixed',
				left: '200%'
			});
		}
		var mdiv_owidth = mdiv.outerWidth();

		// Get the client width
		var cw = mdiv.parent()[0].clientWidth;
		if (cw === undefined) {
			mdiv.css({
				'min-width': '100%',
				'max-width': '100%'
			});
			cw = mdiv.outerWidth();
		}

		if (oldmodal_css.display === 'none')
			mdiv.parent().css(oldmodal_css);

		var p = Math.min(100, 100 * (mdiv_owidth + 2) / cw);
		mdiv.css({
			'min-width': 'calc('.concat(p + '% - 2px)'),
			'max-width': ''
		});
	}

	var click_handler = function(handler) {
		return function(e) {
			if (!$(this).hasClass('active')) {
				set_min_width(this);
				$(this).parent().children('.active').removeClass('active');
				$(this).addClass('active');

				var elem = this;
				History.replace_current(History.get_current_view_elem(),
						document.URL.substring(0, document.URL.length - window.location.hash.length) + prior_hash + '#' + tabname_to_hash($(elem).text()));
				old_url_hash_parser.assign_as_parsed(window.location.hash);
				res.trigger('tabmenuTabHasChanged', elem);
				handler.call(elem);
				centerize_oldmodal($(elem).parents('.oldmodal'), false);
			}
			e.preventDefault();
		};
	};
	for (var i = 0; i < tabs.length; i += 2)
		res.append($('<a>', {
			href: document.URL.substring(0, document.URL.length - window.location.hash.length) + prior_hash + '#' + tabname_to_hash(tabs[i]),
			text: tabs[i],
			click: click_handler(tabs[i + 1])
		}));

	attacher(res);

	var arg = old_url_hash_parser.extract_next_arg();
	var rc = res.children();
	for (var i = 0; i < rc.length; ++i) {
		var elem = $(rc[i]);
		if (tabname_to_hash(elem.text()) === arg) {
			set_min_width(this);
			elem.addClass('active');
			res.trigger('tabmenuTabHasChanged', elem);

			tabs[i << 1 | 1].call(elem);
			centerize_oldmodal(elem.parents('.oldmodal'), false);
			return;
		}
	}

	rc.first().click();
}

/* ================================== OldView ================================== */
function view_base(as_oldmodal, new_window_location, func, no_oldmodal_elem) {
	if (as_oldmodal) {
		var elem = $('<div>', {
			class: 'oldmodal oldview',
			style: 'display: none',
			html: $('<div>', {
				class: 'window',
				html: $('<span>', { class: 'close'})
				.add('<div>', {style: 'display:block'})
			})
		});

		History.push(elem[0], new_window_location);

		elem.appendTo('body').fadeIn(fade_in_duration);
		func.call(elem.find('div > div'));
		centerize_oldmodal(elem);

	// Use body as the parent element
	} else if (no_oldmodal_elem === undefined || $(no_oldmodal_elem).is('body')) {
		History.replace_current(document.querySelector('.body_view_elem'), new_window_location);
		const elem = document.body.appendChild(elem_with_class('div', 'view'));
		func.call($(elem));

	// Use @p no_oldmodal_elem as the parent element
	} else {
		History.push($(no_oldmodal_elem)[0], new_window_location);
		func.call($(no_oldmodal_elem));
	}
}
function timed_hide(elem, hide_time /*= timed_hide_delay [milliseconds] */) {
	if (hide_time === undefined)
		hide_time = timed_hide_delay;

	$(elem).hide();
	$(elem).each(function() {
		var xx = this;
		xx.show = function() {
			window.clearTimeout(timer);
			$(xx).fadeIn(fade_in_duration);
		};
		var timer = window.setTimeout(xx.show, hide_time);
	});
}
function timed_hide_show(elem) {
	$(elem).each(function() {
		if (this.show !== undefined)
			this.show();
	});
}
function old_view_ajax(as_oldmodal, ajax_url, success_handler, new_window_location, no_oldmodal_elem /*= document.body*/, show_on_success /*= true */) {
	view_base(as_oldmodal, new_window_location, function() {
		var elem = $(this);
		var oldmodal = elem.parent().parent();
		if (as_oldmodal)
			timed_hide(oldmodal);
		old_API_call(ajax_url, function() {
			if (as_oldmodal && (show_on_success !== false))
				timed_hide_show(oldmodal);
			success_handler.apply(elem, arguments);
			if (as_oldmodal)
				centerize_oldmodal(oldmodal);
		}, elem);
	}, no_oldmodal_elem);
}
function view_ajax(as_oldmodal, ajax_url, success_handler, new_window_location, no_oldmodal_elem /*= document.body*/, show_on_success /*= true */) {
	view_base(as_oldmodal, new_window_location, function() {
		var elem = $(this);
		var oldmodal = elem.parent().parent();
		if (as_oldmodal)
			timed_hide(oldmodal);
		API_get(ajax_url, function() {
			if (as_oldmodal && (show_on_success !== false))
				timed_hide_show(oldmodal);
			success_handler.apply(elem, arguments);
			if (as_oldmodal)
				centerize_oldmodal(oldmodal);
		}, elem);
	}, no_oldmodal_elem);
}
function a_view_button(href, text, classes, func) {
	var a = document.createElement('a');
	if (href !== undefined)
		a.href = href;
	a.innerText = text;
	if (classes !== undefined)
		a.className = classes;
	a.onclick = function(event) {
		if (event.ctrlKey)
			return true; // Allow the link to open in a new tab
		func();
		return false;
	};
	return a;
}
function api_request_with_password_to_job(elem, title, api_url, message_html, confirm_text, success_msg, form_elements) {
	var as_oldmodal = elem.closest('.oldmodal').length !== 0;
	elem.append(ajax_form(title, api_url,
		$('<p>', {
			style: 'margin: 0 0 20px; text-align: center; max-width: 420px',
			html: message_html
		}).add(form_elements)
		.add(OldForm.field_group('Your password', {
			type: 'password',
			name: 'password',
			size: 24,
		})).add('<div>', {
			html: $('<input>', {
				class: 'btn red',
				type: 'submit',
				value: confirm_text
			}).add('<a>', {
				class: 'btn',
				href: (as_oldmodal ? undefined : '/'),
				text: 'Go back',
				click: function() {
					var oldmodal = $(this).closest('.oldmodal');
					if (oldmodal.length === 0)
						history.back();
					else
						close_oldmodal(oldmodal);
				}
			})
		}), function(resp, oldloader_parent) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], success_msg);
				view_job(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/jobs/' + resp;
			}
		}
	));
}
function delete_with_password_to_job(elem, title, api_url, message_html, confirm_text) {
	return api_request_with_password_to_job(elem, title, api_url, message_html, confirm_text, 'Deletion has been scheduled.');
}

/* ================================= OldLister ================================= */
function OldLister(elem) {
	var this_ = this;
	this.elem = $(elem);
	var lock = false;

	this.need_to_fetch_more = function () {
		// Iff scrolling down is (almost) not possible, true is returned
		return how_much_is_viewport_bottom_above_elem_bottom(this_.elem[0]) <= 300;
	}

	this.fetch_more = function() {
		if (lock || !$.contains(document.documentElement, this_.elem[0]))
			return;

		lock = true;
		append_oldloader(this_.elem.parent()[0]);

		$.ajax({
			url: this_.query_url + this_.query_suffix,
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			dataType: 'json',
			success: function(data) {
				var oldmodal = this_.elem.parents('.oldmodal');
				data = parse_api_resp(data);
				this_.process_api_response(data, oldmodal);

				try_remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				centerize_oldmodal(oldmodal, false);

				if ((Array.isArray(data) && data.length === 0) || (Array.isArray(data.rows) && data.rows.length === 0))
					return; // No more data to load

				lock = false;
				if (this_.need_to_fetch_more()) {
					setTimeout(this_.fetch_more, 0); // avoid recursion
				}
			},
			error: function(resp, status) {
				show_error_via_oldloader(this_.elem.parent(), resp, status,
					function () {
						lock = false;
						this_.fetch_more();
					});
			}
		});
	};

	this.monitor_scroll = function() {
		var scres_handler;
		var elem_to_listen_on_scroll;
		var scres_unhandle_if_detatched = function() {
			if (!$.contains(document.documentElement, this_.elem[0])) {
				elem_to_listen_on_scroll.off('scroll', scres_handler);
				$(window).off('resize', scres_handler);
				return;
			}
		};
		var oldmodal_parent = this_.elem.closest('.oldmodal');
		elem_to_listen_on_scroll = (oldmodal_parent.length === 1 ? oldmodal_parent : $(document));

		scres_handler = function() {
			scres_unhandle_if_detatched();
			if (this_.need_to_fetch_more())
				this_.fetch_more();
		};

		elem_to_listen_on_scroll.on('scroll', scres_handler);
		$(window).on('resize', scres_handler);
	};
}

/* ================================== Logs ================================== */
function HexToUtf8Parser() {
	var state = 0;
	var value = 0;

	this.feed = function(hexstr) {
		hexstr = hexstr.toString(); // force conversion
		if (hexstr.length % 2 !== 0)
			throw new Error("hexstr has to have even length");

		var res = '';
		for (var i = 0; i < hexstr.length; i += 2) {
			var char = parseInt(hexstr.substring(i, i + 2), 16);
			if (state == 0) {
				// No sequence has started yet
				if (char < 128) { // 128 = 0b10000000
					res += String.fromCharCode(char);
					continue;
				}

				if (char < 192) // 192 == 0b11000000
					continue; // Ignore invalid character

				if (char < 224) { // 224 == 0b11100000
					// 0b110xxxxx <--> one more byte to read
					state = 1;
					value = char & ((1 << 5) - 1);
					continue;
				}

				if (char < 240) { // 240 == 0b11110000
					// 0b1110xxxx <--> two more bytes to read
					state = 2;
					value = char & ((1 << 4) - 1);
					continue;
				}

				if (char < 248) { // 248 == 0b11111000
					// 0b11110xxx <--> two more bytes to read
					state = 3;
					value = char & ((1 << 3) - 1);
					continue;
				}

				continue; // Ignore invalid character
			}

			if (char < 128 || char >= 192) // 192 = 0b11000000
				continue; // Ignore invalid character

			value = (value << 6) | (char & ((1 << 6) - 1));
			if (--state == 0)
				res += String.fromCharCode(value);
		}

		return res;
	}
};
function colorize(log, end) {
	if (end === undefined || end > log.length)
		end = log.length;

	var res = "";
	var opened = null;

	function close_last_tag() {
		if (opened == 'span')
			res += '</span>';
		else if (opened == 'b')
			res += '</b>';
		opened = null;
	}

	function contains(idx, str) {
		return (log.substring(idx, idx + str.length) == str);
	}

	for (var i = 0; i < end; ++i) {
		if (contains(i, '\033[30m')) {
			close_last_tag();
			res += '<span class="gray">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[31m')) {
			close_last_tag();
			res += '<span class="red">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[32m')) {
			close_last_tag();
			res += '<span class="green">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[33m')) {
			close_last_tag();
			res += '<span class="yellow">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[34m')) {
			close_last_tag();
			res += '<span class="blue">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[35m')) {
			close_last_tag();
			res += '<span class="magentapink">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[36m')) {
			close_last_tag();
			res += '<span class="turquoise">';
			opened = 'span';
			i += 4;
		} else if (contains(i, '\033[1;30m')) {
			close_last_tag();
			res += '<b class="gray">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;31m')) {
			close_last_tag();
			res += '<b class="red">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;32m')) {
			close_last_tag();
			res += '<b class="green">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;33m')) {
			close_last_tag();
			res += '<b class="yellow">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;34m')) {
			close_last_tag();
			res += '<b class="blue">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;35m')) {
			close_last_tag();
			res += '<b class="pink">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[1;36m')) {
			close_last_tag();
			res += '<b class="turquoise">';
			opened = 'b';
			i += 6;
		} else if (contains(i, '\033[m') && opened !== null) {
			close_last_tag();
			opened = null;
			i += 2;
		} else
			res += log[i];
	}
	close_last_tag();

	return res + log.substring(end);
}
// Scroll: overflowed elements
function is_overflowed_elem_scrolled_down(elem) {
	const scroll_distance_to_bottom = elem.scrollHeight - elem.scrollTop - elem.clientHeight;
	return scroll_distance_to_bottom <= 1; // As of March 2020, I managed to get value 1 in firefox with 80% zoom
}
function Logs(type, elem, auto_refresh_checkbox) {
	var this_ = this;
	this.type = type;
	this.elem = $(elem);
	this.offset = undefined;
	var lock = false; // allow only manual unlocking
	var offset, first_offset;
	var content = $('<span>').appendTo(elem);
	var hex_parser = new HexToUtf8Parser();

	var process_data = function(data) {
		data[0] = parseInt(data[0]);
		if (first_offset === undefined)
			first_offset = data[0];
		else if (data[0] > first_offset) { // Newer logs arrived
			first_offset = data[0];
			content.empty();
		}

		offset = data[0];
		data = hex_parser.feed(data[1]);

		var prev_height = content[0].scrollHeight;
		var bottom_dist = prev_height - content[0].scrollTop;

		remove_oldloader(this_.elem[0]);
		var html_data = text_to_safe_html(data);
		content.html(colorize(html_data + content.html(), html_data.length + 2000));
		var curr_height = content[0].scrollHeight;
		content.scrollTop(curr_height - bottom_dist);

		lock = false;

		// Load more logs unless scrolling up became impossible
		if (offset > 0 && (content.innerHeight() >= curr_height || prev_height == curr_height)) {
			// avoid recursion
			setTimeout(this_.fetch_more, 0);
		}
	};

	this.fetch_more = function() {
		if (lock || !$.contains(document.documentElement, this_.elem[0]))
			return;

		lock = true;

		append_oldloader(this_.elem[0]);
		$.ajax({
			url: '/api/logs/' + this_.type +
				(offset === undefined ? '' : '?' + offset),
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			success: function (data) {
				process_data(String(data).split('\n'));
			},
			error: function(resp, status) {
				show_error_via_oldloader(this_.elem, resp, status, function () {
					lock = false; // allow only manual unlocking
					this_.fetch_more();
				});
			}
		});
	};

	this.try_fetching_newest = function() {
		if (lock)
			return;
		lock = true;

		append_oldloader(this_.elem[0]);
		$.ajax({
			url: '/api/logs/' + this_.type,
			type: 'POST',
			processData: false,
			contentType: false,
			data: new FormData(add_csrf_token_to($('<form>')).get(0)),
			success: function(data) {
				data = String(data).split('\n');
				if (parseInt(data[0]) !== first_offset)
					return process_data(data);

				remove_oldloader(this_.elem[0]);
				lock = false;
			},
			error: function(resp, status) {
				show_error_via_oldloader(this_.elem, resp, status, function () {
					lock = false; // allow only manual unlocking
					this_.fetch_more();
				});
			}
		});
	};

	var refreshing_interval_id = setInterval(function() {
		if (!$.contains(document.documentElement, this_.elem[0])) {
			clearInterval(refreshing_interval_id);
			return;
		}

		var elem = content[0];
		if (is_overflowed_elem_scrolled_down(elem) && auto_refresh_checkbox.prop('checked')) {
			this_.try_fetching_newest();
		}
	}, 2000);

	this.monitor_scroll = function() {
		var scres_handler;
		var scres_unhandle_if_detatched = function() {
			if (!$.contains(document.documentElement, this_.elem[0])) {
				content.off('scroll', scres_handler);
				$(window).off('resize', scres_handler);
				return;
			}
		};

		scres_handler = function() {
			scres_unhandle_if_detatched();
			if (content[0].scrollTop <= 300)
				this_.fetch_more();
		};

		content.on('scroll', scres_handler);
		$(window).on('resize', scres_handler);
	};

	this.fetch_more();
}
function tab_logs_view(parent_elem) {
	// Select job server log by default
	if (old_url_hash_parser.next_arg() === '')
		old_url_hash_parser.assign('#job_server');

	parent_elem = $(parent_elem);
	function retab(log_type, log_name) {
		var checkbox = $('<input>', {
			type: 'checkbox',
			checked: (log_type !== 'web')
		});

		$('<div>', {
			class: 'logs-header',
			html: [
				$('<h2>', {text: log_name + ' log:'}),
				$('<div>', {html:
					$('<label>', {html: [
						checkbox, 'auto-refresh'
					]})
				})
			]
		}).appendTo(parent_elem);

		parent_elem.addClass('logs-parent');
		new Logs(log_type, $('<pre>', {class: 'logs'}).appendTo(parent_elem), checkbox).monitor_scroll();
	}

	var tabs = [
		'Server (web)', retab.bind(null, 'web', "Server's"),
		'Server error (web)', retab.bind(null, 'web_err', "Server's error"),
		'Job server', retab.bind(null, 'jobs', "Job server's"),
		'Job server error', retab.bind(null, 'jobs_err', "Job server's error")
	];

	old_tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ============================ Actions buttons ============================ */
const ActionsToHTML = {};

ActionsToHTML.user = function(user, viewing_user = false) {
	let res = [];
	if (!viewing_user && user.capabilities.view) {
		res.push(a_view_button(url_user(user.id), 'View', 'btn-small',
			view_user.bind(null, true, user.id)));
	}
	if (user.capabilities.edit) {
		res.push(a_view_button(url_user_edit(user.id), 'Edit',
			'btn-small blue', edit_user.bind(null, user.id)));
	}
	if (user.capabilities.delete) {
		res.push(a_view_button(url_user_delete(user.id), 'Delete',
			'btn-small red', delete_user.bind(null, user.id)));
	}
	if (user.capabilities.merge_into_another_user) {
		res.push(a_view_button(url_user_merge_into_another(user.id),
			'Merge', 'btn-small red',
			merge_user.bind(null, user.id)));
	}
	if (user.capabilities.change_password || user.capabilities.change_password_without_old_password) {
		res.push(a_view_button(url_change_user_password(user.id),
			'Change password', 'btn-small orange',
			change_user_password.bind(null, user.id)));
	}
	return res;
};

ActionsToHTML.problem = function(problem, viewing_problem = false) {
	let res = [];
	if (!viewing_problem && problem.capabilities.view) {
		res.push(a_view_button(url_problem(problem.id), 'View', 'btn-small',
			view_problem.bind(null, true, problem.id)));
	}
	if (problem.capabilities.view_statement) {
		const a = elem_with_class_and_text('a', 'btn-small', 'Statement');
		a.href = url_problem_statement(problem.id, problem.name);
		res.push(a);
	}
	if (problem.capabilities.create_submission) {
		res.push(a_view_button(url_problem_create_submission(problem.id), 'Submit',
			'btn-small blue', add_problem_submission.bind(null, true, {id: problem.id})));
	}
	if (problem.capabilities.view_solutions) {
		res.push(a_view_button(url_problem_solutions(problem.id),
			'Solutions', 'btn-small', view_problem.bind(null, true, problem.id,
				'#all_submissions#solutions')));
	}
	if (viewing_problem && problem.capabilities.download) {
		const a = elem_with_class_and_text('a', 'btn-small', 'Download');
		a.href = url_problem_download(problem.id);
		res.push(a);
	}
	if (problem.capabilities.edit) {
		res.push(a_view_button(url_problem_edit(problem.id), 'Edit',
			'btn-small blue', edit_problem.bind(null, true, problem.id)));
	}
	if (viewing_problem && problem.capabilities.delete) {
		res.push(a_view_button(url_problem_delete(problem.id), 'Delete',
			'btn-small red', delete_problem.bind(null, true, problem.id)));
	}
	if (viewing_problem && problem.capabilities.merge) {
		res.push(a_view_button(url_problem_merge(problem.id), 'Merge',
			'btn-small red', merge_problem.bind(null, true, problem.id)));
	}
	if (viewing_problem && problem.capabilities.rejudge_all_submissions) {
		const a = elem_with_class_and_text('a', 'btn-small blue', 'Rejudge all submissions');
		a.addEventListener('click', rejudge_problem_submissions.bind(null, problem.id, problem.name), {passive: true});
		res.push(a);
	}
	if (viewing_problem && problem.capabilities.reset_time_limits) {
		res.push(a_view_button(url_problem_reset_time_limits(problem.id), 'Reset time limits',
			'btn-small blue', reset_problem_time_limits.bind(null, true, problem.id)));
	}
	return res;
};

ActionsToHTML.submission = function(submission, viewing_submission = false) {
	let res = [];
	if (!viewing_submission && submission.capabilities.view) {
		res.push(a_view_button(url_submission(submission.id), 'View', 'btn-small', view_submission.bind(null, true, submission.id)));
	}
	if (!viewing_submission && submission.capabilities.download) {
		res.push(a_view_button(url_submission_source(submission.id), 'Source', 'btn-small', view_submission.bind(null, true, submission.id, '#source')));
	}
	if (submission.capabilities.change_type) {
		res.push(a_view_button(undefined, 'Change type', 'btn-small orange', submission_chtype.bind(null, submission.id, submission.type)));
	}
	if (submission.capabilities.rejudge) {
		res.push(a_view_button(undefined, 'Rejudge', 'btn-small blue', rejudge_submission.bind(null, submission.id)));
	}
	if (submission.capabilities.delete) {
		res.push(a_view_button(undefined, 'Delete', 'btn-small red', delete_submission.bind(null, submission.id)));
	}
	return res;
};

ActionsToHTML.job = function(job, viewing_job = false) {
	let res = [];
	if (!viewing_job && job.capabilities.view) {
		res.push(a_view_button(url_job(job.id), 'View', 'btn-small', view_job.bind(null, true, job.id)));
	}
	if (!viewing_job && (job.capabilities.view_log || job.capabilities.download_attached_file)) {
		const dropdown_menu = elem_with_class('div', 'dropmenu down');
		dropdown_menu.appendChild(elem_with_class_and_text('a', 'btn-small dropmenu-toggle', 'Download'));

		const ul = dropdown_menu.appendChild(document.createElement('ul'));
		if (job.capabilities.view_log) {
			ul.appendChild(elem_with_text('a', 'Job log')).href = url_api_job_download_log(job.id);
		}
		if (job.capabilities.download_attached_file) {
			switch (job.type) {
			case 'add_problem':
			case 'reupload_problem':
				ul.appendChild(elem_with_text('a', 'Uploaded package')).href = '/api/download/job/' + job.id + '/uploaded-package'; // TODO: refactor to attached_file
				break;
			case 'change_problem_statement':
				ul.appendChild(elem_with_text('a', 'Uploaded statement')).href = '/api/download/job/' + job.id + '/uploaded-statement'; // TODO: refactor to attached_file
				break;
			}
		}

		res.push(dropdown_menu);
	}
	if (job.capabilities.change_priority && job.status == 'pending') {
		// TODO: implement when changing priority will be implemented (probably blue color will be good)
	}
	if (job.capabilities.cancel && job.status == 'pending') {
		res.push(a_view_button(undefined, 'Cancel', 'btn-small red', cancel_job.bind(null, job.id))); // TODO: refactor
	}
	if (job.capabilities.restart && (job.status == 'failed' || job.status == 'cancelled')) {
		res.push(a_view_button(undefined, 'Restart', 'btn-small orange', restart_job.bind(null, job.id)));
	}
	return res;
}

ActionsToHTML.oldjob = function(job_id, actions_str, problem_id, job_view /*= false*/) {
	if (job_view == undefined)
		job_view = false;

	var res = [];
	if (!job_view && actions_str.indexOf('v') !== -1)
		res.push(a_view_button('/jobs/' + job_id, 'View', 'btn-small',
			view_job.bind(null, true, job_id)));

	if (actions_str.indexOf('u') !== -1 || actions_str.indexOf('r') !== -1)
		res.push($('<div>', {
			class: 'dropmenu down',
			html: $('<a>', {
				class: 'btn-small dropmenu-toggle',
				text: 'Download'
			}).add('<ul>', {
				html: [actions_str.indexOf('r') === -1 ? '' :  $('<a>', {
						href: '/api/download/job/' + job_id + '/log',
						text: 'Job log'
					}), actions_str.indexOf('u') === -1 ? '' : $('<a>', {
						href: '/api/download/job/' + job_id + '/uploaded-package',
						text: 'Uploaded package'
					}), actions_str.indexOf('s') === -1 ? '' : $('<a>', {
						href: '/api/download/job/' + job_id + '/uploaded-statement',
						text: 'Uploaded statement'
					})
				]
			})
		}));

	if (actions_str.indexOf('p') !== -1)
		res.push(a_view_button('/p/' + problem_id, 'View problem',
			'btn-small green', view_problem.bind(null, true, problem_id)));

	if (actions_str.indexOf('C') !== -1)
		res.push($('<a>', {
			class: 'btn-small red',
			text: 'Cancel job',
			click: cancel_job.bind(null, job_id)
		}));

	if (actions_str.indexOf('R') !== -1)
		res.push(a_view_button(undefined, 'Restart job', 'btn-small orange',
			restart_job.bind(null, job_id)));

	return res;
};

ActionsToHTML.oldsubmission = function(submission_id, actions_str, submission_type, submission_view /*= false*/) {
	if (submission_view === undefined)
		submission_view = false;

	var res = [];
	if (!submission_view && actions_str.indexOf('v') !== -1)
		res.push(a_view_button('/s/' + submission_id, 'View', 'btn-small',
			view_submission.bind(null, true, submission_id)));

	if (actions_str.indexOf('s') !== -1) {
		if (!submission_view)
			res.push(a_view_button('/s/' + submission_id + '#source', 'Source',
				'btn-small', view_submission.bind(null, true, submission_id, '#source')));

		if (submission_view) {
			var a = document.createElement('a');
			a.className = 'btn-small';
			a.href = '/api/download/submission/' + submission_id;
			a.innerText = 'Download';
			res.push(a);
		}
	}

	if (actions_str.indexOf('C') !== -1)
		res.push(a_view_button(undefined, 'Change type', 'btn-small orange',
			submission_chtype.bind(null, submission_id, submission_type)));

	if (actions_str.indexOf('R') !== -1)
		res.push(a_view_button(undefined, 'Rejudge', 'btn-small blue',
			rejudge_submission.bind(null, submission_id)));

	if (actions_str.indexOf('D') !== -1)
		res.push(a_view_button(undefined, 'Delete', 'btn-small red',
			delete_submission.bind(null, submission_id)));

	return res;
};

ActionsToHTML.oldproblem = function(problem, problem_view /*= false*/) {
	if (problem_view === undefined)
		problem_view = false;

	var res = [];
	if (!problem_view && problem.actions.indexOf('v') !== -1)
		res.push(a_view_button('/p/' + problem.id, 'View', 'btn-small',
			view_problem.bind(null, true, problem.id)));

	if (problem.actions.indexOf('V') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/api/download/statement/problem/' + problem.id + '/' + encodeURIComponent(problem.name),
			text: 'Statement'
		}));

	if (problem.actions.indexOf('S') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/submit', 'Submit',
			'btn-small blue', add_problem_submission.bind(null, true, problem)));

	if (problem.actions.indexOf('s') !== -1)
		res.push(a_view_button('/p/' + problem.id + '#all_submissions#solutions',
			'Solutions', 'btn-small', view_problem.bind(null, true, problem.id,
				'#all_submissions#solutions')));

	if (problem_view && problem.actions.indexOf('d') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/api/download/problem/' + problem.id,
			text: 'Download'
		}));

	if (problem.actions.indexOf('E') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/edit', 'Edit',
			'btn-small blue', edit_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('R') !== -1)
		res.push(elem_link_with_class_to_view('btn-small orange', 'Reupload', reupload_problem, url_problem_reupload, problem.id));

	if (problem_view && problem.actions.indexOf('D') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/delete', 'Delete',
			'btn-small red', delete_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('M') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/merge', 'Merge',
			'btn-small red', merge_problem.bind(null, true, problem.id)));

	if (problem_view && problem.actions.indexOf('J') !== -1)
		res.push($('<a>', {
			class: 'btn-small blue',
			text: 'Rejudge all submissions',
			click: rejudge_problem_submissions.bind(null, problem.id, problem.name)
		}));

	if (problem_view && problem.actions.indexOf('L') !== -1)
		res.push(a_view_button('/p/' + problem.id + '/reset_time_limits',
			'Reset time limits', 'btn-small blue',
			reset_problem_time_limits.bind(null, true, problem.id)));

	return res;
};

ActionsToHTML.contest = function(contest_id, actions_str, contest_view /*= false*/) {
	if (contest_view === undefined)
		contest_view = false;

	var res = [];
	if (!contest_view && actions_str.indexOf('v') !== -1)
		res.push($('<a>', {
			class: 'btn-small',
			href: '/c/c' + contest_id,
			text: 'View'
		}), $('<a>', {
			class: 'btn-small',
			href: '/c/c' + contest_id + '#ranking',
			text: 'Ranking'
		}));

	if (contest_view && actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/edit', 'Edit',
			'btn-small blue', edit_contest.bind(null, true, contest_id)));

	if (contest_view && actions_str.indexOf('D') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/delete', 'Delete',
			'btn-small red', delete_contest.bind(null, true, contest_id)));

	return res;
};

ActionsToHTML.contest_user = function(user_id, contest_id, actions_str) {
	var res = [];
	var make_contestant = (actions_str.indexOf('Mc') !== -1);
	var make_moderator = (actions_str.indexOf('Mm') !== -1);
	var make_owner = (actions_str.indexOf('Mo') !== -1);

	if (make_contestant + make_moderator + make_owner > 1)
		res.push(a_view_button('/c/c' + contest_id + '/contest_user/' + user_id + '/change_mode', 'Change mode', 'btn-small orange',
			change_contest_user_mode.bind(null, true, contest_id, user_id)));

	if (actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/c/c' + contest_id + '/contest_user/' + user_id + '/expel',
			'Expel', 'btn-small red',
			expel_contest_user.bind(null, true, contest_id, user_id)));

	return res;
};

ActionsToHTML.contest_users = function(contest_id, overall_actions_str) {
	var add_contestant = (overall_actions_str.indexOf('Ac') !== -1);
	var add_moderator = (overall_actions_str.indexOf('Am') !== -1);
	var add_owner = (overall_actions_str.indexOf('Ao') !== -1);

	if (add_contestant || add_moderator || add_owner)
		return a_view_button('/c/c' + contest_id + '/contest_user/add',
			'Add user', 'btn', add_contest_user.bind(null, true, contest_id));

	return [];
};

ActionsToHTML.contest_file = function(contest_file_id, actions_str) {
	var res = [];

	if (actions_str.indexOf('O') !== -1) {
		var a = document.createElement('a');
		a.className = 'btn-small';
		a.href = '/api/download/contest_file/' + contest_file_id;
		a.innerText = 'Download';
		res.push(a);
	}

	if (actions_str.indexOf('E') !== -1)
		res.push(a_view_button('/contest_file/' + contest_file_id + '/edit',
			'Edit', 'btn-small blue', edit_contest_file.bind(null, true, contest_file_id)));

	if (actions_str.indexOf('D') !== -1)
		res.push(a_view_button('/contest_file/' + contest_file_id + '/delete',
			'Delete', 'btn-small red', delete_contest_file.bind(null, true, contest_file_id)));

	return res;
};

ActionsToHTML.contest_files = function(contest_id, overall_actions_str) {
	if (overall_actions_str.indexOf('A') !== -1)
		return a_view_button('/c/c' + contest_id + '/files/add',
			'Add file', 'btn', add_contest_file.bind(null, true, contest_id));

	return [];
};

/* ================================= Users ================================= */
function view_user(as_oldmodal, user_id, opt_hash /*= ''*/) {
	view_ajax(as_oldmodal, url_api_user(user_id), function(user) {
		this.append($('<div>', {
			class: 'header',
			html: $('<span>', {
				style: 'margin: auto 0',
				html: $('<a>', {
					href: url_user(user.id),
					text: user.username
				})
			}).append(text_to_safe_html(' (' + user.first_name + ' ' + user.last_name + ')'))
			.add('<div>', {
				html: ActionsToHTML.user(user, true)
			})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'user-info',
				html: $('<div>', {
					class: 'first-name',
					html: $('<label>', {text: 'First name'})
				}).append(text_to_safe_html(user.first_name))
				.add($('<div>', {
					class: 'last-name',
					html: $('<label>', {text: 'Last name'})
				}).append(text_to_safe_html(user.last_name)))
				.add($('<div>', {
					class: 'username',
					html: $('<label>', {text: 'Username'})
				}).append(text_to_safe_html(user.username)))
				.add($('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Account type'}).add('<span>', {
						class: user.type,
						text: user.type[0].toUpperCase() + user.type.slice(1),
					})
				}))
				.add($('<div>', {
					class: 'email',
					html: $('<label>', {text: 'Email'})
				}).append(text_to_safe_html(user.email)))
			})
		}));

		var main = $(this);

		old_tabmenu(default_tabmenu_attacher.bind(main), [
			'Submissions', function() {
				main.append($('<div>', {html: "<h2>User's submissions</h2>"}));
				tab_submissions_lister(main.children().last(), '/u' + user_id, false, !signed_user_is_admin());
			},
			'Jobs', function() {
				var j_table = $('<table>', {
					class: 'jobs'
				});
				main.append($('<div>', {
					html: ["<h2>User's jobs</h2>", j_table]
				}));
				new OldJobsLister(j_table, '/u' + user_id).monitor_scroll();
			}
		]);

	}, url_user(user_id) + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}

/* ================================== Jobs ================================== */
function view_job(as_oldmodal, job_id, opt_hash /*= ''*/) {
	old_view_ajax(as_oldmodal, '/api/jobs/=' + job_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var job = data[0];
		function info_html(info) {
			var td = $('<td>', {
				style: 'text-align:left'
			});
			for (var name in info) {
				td.append($('<label>', {text: name}));

				if (name == "submission")
					td.append(a_view_button('/s/' + info[name], info[name],
						undefined, view_submission.bind(null, true, info[name])));
				else if (name == "user" || name == "deleted user" || name == "target user")
					td.append(a_view_button(url_user(info[name]), info[name],
						undefined, view_user.bind(null, true, info[name])));
				else if (name == "problem" || name == "deleted problem" || name == "target problem")
					td.append(a_view_button('/p/' + info[name], info[name],
						undefined, view_problem.bind(null, true, info[name])));
				else if (name == "contest")
					td.append(a_view_button('/c/c' + info[name], info[name],
						undefined, view_contest.bind(null, true, info[name])));
				else if (name == "contest round")
					td.append(a_view_button('/c/r' + info[name], info[name],
						undefined, view_contest_round.bind(null, true, info[name])));
				else if (name == "contest problem")
					td.append(a_view_button('/c/p' + info[name], info[name],
						undefined, view_contest_problem.bind(null, true, info[name])));
				else
					td.append(info[name]);

				td.append('<br>');
			}

			return td;
		}

		this.append($('<div>', {
			class: 'job-info',
			html: $('<div>', {
				class: 'header',
				html: $('<h1>', {
					text: 'Job ' + job_id
				}).add('<div>', {
					html: ActionsToHTML.oldjob(job_id, job.actions, job.info.problem, true)
				})
			}).add('<table>', {
				html: $('<thead>', {html: '<tr>' +
					'<th style="min-width:120px">Type</th>' +
					'<th style="min-width:150px">Added</th>' +
					'<th style="min-width:150px">Status</th>' +
					'<th style="min-width:120px">Owner</th>' +
					'<th style="min-width:90px">Info</th>' +
					'</tr>'
				}).add('<tbody>', {
					html: $('<tr>', {
						html: $('<td>', {
							text: job.type
						}).add(normalize_datetime($('<td>', {
								datetime: job.created_at,
								text: job.created_at
							}), true)
						).add('<td>', {
							class: 'status ' + job.status.class,
							text: job.status.text
						}).add('<td>', {
							html: job.creator_id === null ? 'System' :
								(job.creator_username == null ? 'Deleted (id: ' + job.creator_id + ')'
								: a_view_button(
								url_user(job.creator_id), job.creator_username, undefined,
								view_user.bind(null, true, job.creator_id)))
						}).add('<td>', {
							html: info_html(job.info)
						})
					})
				})
			})
		}));

		if (job.actions.indexOf('r') !== -1) {
			this.append('<h2>Job log</h2>')
			.append($('<pre>', {
				class: 'job-log',
				html: colorize(text_to_safe_html(job.log.text))
			}));

			if (job.log.is_incomplete)
				this.append($('<p>', {
					text: 'The job log is too large to show it entirely here. If you want to see the whole, click: '
				}).append($('<a>', {
					class: 'btn-small',
					href: '/api/download/job/' + job_id + '/log',
					text: 'Download the full job log'
				})));
		}
	}, '/jobs/' + job_id + (opt_hash === undefined ? '' : opt_hash));
}
function cancel_job(job_id) {
	oldmodal_request('Canceling job ' + job_id, $('<form>'),
		'/api/job/' + job_id + '/cancel', 'The job has been cancelled.');
}
function restart_job(job_id) {
	dialogue_oldmodal_request('Restart job', $('<label>', {
			html: [
				'Are you sure to restart the ',
				a_view_button('/jobs/' + job_id, 'job ' + job_id, undefined,
					view_job.bind(null, true, job_id)),
				'?'
			]
		}), 'Restart job', 'btn-small orange', '/api/job/' + job_id + '/restart',
		'The job has been restarted.', 'No, go back');
}
function OldJobsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/jobs' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, oldmodal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'jobs always_in_view',
					// class: 'jobs',
					html: '<p>There are no jobs to show...</p>'
				}));
				remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="type">Type</th>' +
					'<th class="priority">Priority</th>' +
					'<th class="created_at">Created at</th>' +
					'<th class="status">Status</th>' +
					'<th class="creator">Creator</th>' +
					'<th class="info">Info</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.created_at'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');
			row.append($('<td>', {text: x.id}));
			row.append($('<td>', {text: x.type}));
			row.append($('<td>', {text: x.priority}));

			var avb = a_view_button('/jobs/' + x.id, x.created_at, undefined,
				view_job.bind(null, true, x.id));
			avb.setAttribute('datetime', x.created_at);
			row.append($('<td>', {
				html: normalize_datetime(avb, false)
			}));
			row.append($('<td>', {
				class: 'status ' + x.status.class,
				text: x.status.text
			}));
			row.append($('<td>', {
				html: x.creator_id === null ? 'System' : (x.creator_username == null ? x.creator_id
					: a_view_button(url_user(x.creator_id), x.creator_username, undefined,
						view_user.bind(null, true, x.creator_id)))
			}));
			// Info
			var info = x.info;
			{
				/* jshint loopfunc: true */
				var td = $('<td>');
				var div = $('<div>', {
					class: 'info'
				}).appendTo(td);
				var append_tag = function(name, val) {
					div.append($('<label>', {text: name}));
					div.append(val);
				};

				if (info.submission !== undefined)
					append_tag('submission',
						a_view_button('/s/' + info.submission,
							info.submission, undefined,
							view_submission.bind(null, true, info.submission)));

				if (info.user !== undefined)
					append_tag('user',
						a_view_button(url_user(info.user),
							info.user, undefined,
							view_user.bind(null, true, info.user)));

				if (info["deleted user"] !== undefined)
					append_tag('deleted user',
						a_view_button(url_user(info["deleted user"]),
							info["deleted user"], undefined,
							view_user.bind(null, true, info["deleted user"])));

				if (info["target user"] !== undefined)
					append_tag('target user',
						a_view_button(url_user(info["target user"]),
							info["target user"], undefined,
							view_user.bind(null, true, info["target user"])));

				if (info.problem !== undefined)
					append_tag('problem',
						a_view_button('/p/' + info.problem,
							info.problem, undefined,
							view_problem.bind(null, true, info.problem)));

				if (info['deleted problem'] !== undefined)
					append_tag('deleted problem',
						a_view_button('/c/p' + info['deleted problem'],
							info['deleted problem'], undefined,
							view_problem.bind(null, true, info['deleted problem'])));

				if (info['target problem'] !== undefined)
					append_tag('target problem',
						a_view_button('/c/p' + info['target problem'],
							info['target problem'], undefined,
							view_problem.bind(null, true, info['target problem'])));

				if (info.contest !== undefined)
					append_tag('contest',
						a_view_button('/c/c' + info.contest,
							info.contest, undefined,
							view_contest.bind(null, true, info.contest)));

				if (info['contest problem'] !== undefined)
					append_tag('contest problem',
						a_view_button('/c/p' + info['contest problem'],
							info['contest problem'], undefined,
							view_contest_problem.bind(null, true, info['contest problem'])));

				if (info['contest round'] !== undefined)
					append_tag('contest round',
						a_view_button('/c/r' + info['contest round'],
							info['contest round'], undefined,
							view_contest_round.bind(null, true, info['contest round'])));

				var names = ['name', 'memory limit', 'problem type'];
				for (var idx in names) {
					var name = names[idx];
					if (info[name] !== undefined)
						append_tag(name, info[name]);
				}

				row.append(td);
			}

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.oldjob(x.id, x.actions, info.problem)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_jobs_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="jobs"></table>').appendTo(parent_elem);
		new OldJobsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'My', retab.bind(null, '/u' + signed_user_id)
	];

	old_tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ============================== Submissions ============================== */
function add_submission_impl(as_oldmodal, url, api_url, problem_field_elem, maybe_ignored, ignore_by_default, no_oldmodal_elem) {
	view_base(as_oldmodal, url, function() {
		this.append(ajax_form('Submit a solution', api_url,
			OldForm.field_group('Problem', problem_field_elem
			).add(OldForm.field_group('Language',
				$('<select>', {
					name: 'language',
					required: true,
					html: $('<option>', {
						value: 'c11',
						text: 'C11'
					}).add('<option>', {
						value: 'cpp11',
						text: 'C++11',
					}).add('<option>', {
						value: 'cpp14',
						text: 'C++14'
					}).add('<option>', {
						value: 'cpp17',
						text: 'C++17',
					}).add('<option>', {
						value: 'cpp20',
						text: 'C++20',
						selected: true
					}).add('<option>', {
						value: 'pascal',
						text: 'Pascal'
					}).add('<option>', {
						value: 'python',
						text: 'Python'
					}).add('<option>', {
						value: 'rust',
						text: 'Rust'
					})
				})
			)).add(OldForm.field_group('Solution', {
				type: 'file',
				name: 'solution',
			})).add(OldForm.field_group('Code',
				$('<textarea>', {
					class: 'monospace',
					name: 'code',
					rows: 8,
					cols: 50
				})
			)).add(maybe_ignored ? OldForm.field_group('Ignored submission', {
				type: 'checkbox',
				name: 'ignored',
				checked: ignore_by_default
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_oldmodal) {
					show_success_via_oldloader($(this)[0], 'Submitted');
					view_submission(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/s/' + resp;
				}
			})
		);
	}, no_oldmodal_elem);
}
function add_problem_submission(as_oldmodal, problem, no_oldmodal_elem /*= undefined*/) {
	if (problem.name === undefined) { // Request server for all needed data
		old_view_ajax(as_oldmodal, "/api/problems/=" + problem.id, function(data) {
			if (data.length === 0)
				return show_error_via_oldloader(this, {
					status: '404',
					statusText: 'Not Found'
				});

			if (as_oldmodal)
				close_oldmodal($(this).closest('.oldmodal'));

			add_problem_submission(as_oldmodal, data[0]);
		}, '/p/' + problem.id + '/submit');
		return;
	}

	add_submission_impl(as_oldmodal, '/p/' + problem.id + '/submit',
		'/api/submission/add/p' + problem.id,
		a_view_button('/p/' + problem.id, problem.name, undefined, view_problem.bind(null, true, problem.id)),
		(problem.actions.indexOf('i') !== -1), false,
		no_oldmodal_elem);
}
function add_contest_submission(as_oldmodal, contest, round, problem, no_oldmodal_elem /*= undefined*/) {
	if (contest === undefined) { // Request server for all needed data
		old_view_ajax(as_oldmodal, "/api/contest/p" + problem.id, function(data) {
			if (as_oldmodal)
				close_oldmodal($(this).closest('.oldmodal'));

			add_contest_submission(as_oldmodal, data.contest, data.rounds[0], data.problems[0]);
		}, '/c/p' + problem.id + '/submit');
		return;
	}

	add_submission_impl(as_oldmodal, '/c/p' + problem.id + '/submit',
		'/api/submission/add/p' + problem.problem_id + '/cp' + problem.id,
		$('<div>', {
			class: 'contest-path',
			html: [
				a_view_button('/c/c' + contest.id, contest.name, '',
					view_contest.bind(null, true, contest.id)),
				' / ',
				a_view_button('/c/r' + round.id, round.name, '',
					view_contest_round.bind(null, true, round.id)),
				' / ',
				a_view_button('/c/p' + problem.id, problem.name, '',
					view_contest_problem.bind(null, true, problem.id))
			]
		}), (contest.actions.indexOf('A') !== -1),
		(contest.actions.indexOf('A') !== -1), no_oldmodal_elem);
}

function rejudge_submission(submission_id) {
	oldmodal_request('Scheduling submission rejudge ' + submission_id, $('<form>'),
		'/api/submission/' + submission_id + '/rejudge',
		'The rejudge has been scheduled.');
}
function submission_chtype(submission_id, submission_type) {
	dialogue_oldmodal_request('Change submission type', $('<form>', {
		html: [
			$('<label>', {
				html: [
					'New type of the ',
					a_view_button('/s/' + submission_id, 'submission ' + submission_id,
						undefined, view_submission.bind(null, true, submission_id)),
					': '
				]
			}),
			$('<select>', {
				style: 'margin-left: 4px',
				name: 'type',
				html: [
					$('<option>', {
						value: 'N',
						text: 'Normal / final',
						selected: (submission_type !== 'Ignored' && submission_type !== 'ignored' ? true : undefined)
					}),
					$('<option>', {
						value: 'I',
						text: 'Ignored',
						selected: (submission_type === 'Ignored' || submission_type == 'ignored' ? true : undefined)
					})
				]
			})
		]
	}), 'Change type', 'btn-small orange', '/api/submission/' + submission_id + '/chtype',
		'Type of the submission has been updated.', 'No, go back');
}
function delete_submission(submission_id) {
	dialogue_oldmodal_request('Delete submission', $('<label>', {
			html: [
				'Are you sure to delete the ',
				a_view_button('/s/' + submission_id, 'submission ' + submission_id,
					undefined, view_submission.bind(null, true, submission_id)),
				'?'
			]
		}), 'Yes, delete it', 'btn-small red', '/api/submission/' + submission_id + '/delete',
		'The submission has been deleted.', 'No, go back');
}
function view_submission(as_oldmodal, submission_id, opt_hash /*= ''*/) {
	old_view_ajax(as_oldmodal, '/api/submissions/=' + submission_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var s = data[0];

		if (s.contest_id !== null)
			this.append($('<div>', {
				class: 'contest-path',
				html: [
					a_view_button('/c/c' + s.contest_id, s.contest_name, '',
						view_contest.bind(null, true, s.contest_id)),
					' / ',
					a_view_button('/c/r' + s.contest_round_id, s.contest_round_name, '',
						view_contest_round.bind(null, true, s.contest_round_id)),
					' / ',
					a_view_button('/c/p' + s.contest_problem_id, s.contest_problem_name, '',
						view_contest_problem.bind(null, true, s.contest_problem_id)),
					$('<a>', {
						class: 'btn-small',
						href: '/api/download/statement/contest/p' + s.contest_problem_id + '/' +
							encodeURIComponent(s.contest_problem_name),
						text: 'View statement'
					})
				]
			}));

		this.append($('<div>', {
			class: 'submission-info',
			html: [
				$('<div>', {
					class: 'header',
					html: [
						$('<h1>', {
							text: 'Submission ' + submission_id
						}),
						$('<div>', {
							html: ActionsToHTML.oldsubmission(submission_id, s.actions,
								s.type, true)
						})
					]
				}),
				$('<table>', {
					html: [
						$('<thead>', {html: '<tr>' +
							'<th style="min-width:90px">Lang</th>' +
							(s.user_id === null ? ''
								: '<th style="min-width:120px">User</th>') +
							'<th style="min-width:120px">Problem</th>' +
							'<th style="min-width:150px">Submission time</th>' +
							'<th style="min-width:150px">Status</th>' +
							'<th style="min-width:90px">Score</th>' +
							'<th style="min-width:90px">Type</th>' +
							'</tr>'
						}),
						$('<tbody>', {
							html: $('<tr>', {
								class: (s.type === 'Ignored' ? 'ignored' : undefined),
								html: [
									$('<td>', {text: s.language}),
									(s.user_id === null ? '' : $('<td>', {
										html: [a_view_button(url_user(s.user_id), s.user_username,
												undefined, view_user.bind(null, true, s.user_id)),
											' (' + text_to_safe_html(s.user_first_name) + ' ' +
												text_to_safe_html(s.user_last_name) + ')'
										]
									})),
									$('<td>', {
										html: a_view_button('/p/' + s.problem_id, s.problem_name,
											undefined, view_problem.bind(null, true, s.problem_id))
									}),
									normalize_datetime($('<td>', {
										datetime: s.created_at,
										text: s.created_at
									}), true),
									$('<td>', {
										class: 'status ' + s.status.class,
										text: (s.status.class.lastIndexOf('initial') === -1 ?
											'' : 'Initial: ') + s.status.text
									}),
									$('<td>', {text: s.score}),
									$('<td>', {text: s.type})

								]
							})
						})
					]
				})
			]
		}));

		var elem = $(this);
		var tabs = [
			'Reports', function() {
				timed_hide_show(elem.parents('.oldmodal'));
				elem.append($('<div>', {
					class: 'results',
					html: function() {
						var res = [];
						if (s.initial_report != '') {
							res.push($('<h2>', {text: 'Initial judgment protocol'}),
								s.initial_report);
						}
						if (s.final_report != '') {
							res.push($('<h2>', {text: 'Full judgment protocol'}),
								s.final_report);
						}
						if (s.final_report === null) {
							var message_to_show;
							if (utcdt_or_tm_to_Date(s.full_results) === Infinity) {
								message_to_show = $('<p>', {text: 'Full judgment protocol (of all tests) will not be available.'});
							} else {
								message_to_show = $('<p>', {text: 'Full judgment protocol (of all tests) will be visible since: '}).append(normalize_datetime($('<span>', {datetime: s.full_results})));
							}
							res.push(message_to_show);
						}
						return res;
					}()
				}));
			}
		];

		var cached_source;
		if (s.actions.indexOf('s') !== -1)
			tabs.push('Source', function() {
				timed_hide_show(elem.parents('.oldmodal'));
				if (cached_source !== undefined) {
					elem.append(copy_to_clipboard_btn(false, 'Copy to clipboard', function() {
						return $(cached_source).text();
					}));
					elem.append(cached_source);
				} else {
					append_oldloader(elem[0]);
					$.ajax({
						url: '/api/submission/' + submission_id + '/source',
						type: 'POST',
						processData: false,
						contentType: false,
						data: new FormData(add_csrf_token_to($('<form>')).get(0)),
						dataType: 'html',
						success: function(data) {
							cached_source = data;
							elem.append(copy_to_clipboard_btn(false, 'Copy to clipboard', function() {
								return $(cached_source).text();
							}));
							elem.append(cached_source);
							remove_oldloader(elem[0]);
							centerize_oldmodal(elem.closest('.oldmodal'), false);
						},
						error: function(resp, status) {
							show_error_via_oldloader(elem, resp, status);
						}
					});
				}
			});

		if (s.actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				elem.append($('<table>', {class: 'jobs'}));
				new OldJobsLister(elem.children().last(), '/s' + submission_id).monitor_scroll();
			});

		if (s.user_id !== null) {
			tabs.push((s.user_id == signed_user_id ? 'My' : "User's") + ' submissions to this problem', function() {
				elem.append($('<div>'));
				tab_submissions_lister(elem.children().last(), '/u' + s.user_id + (s.contest_id === null ? '/p' + s.problem_id : '/P' + s.contest_problem_id));
			});

			if (s.contest_id !== null && s.user_id != signed_user_id) {
				tabs.push("User's submissions to this round", function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/u' + s.user_id + '/R' + s.contest_round_id);
				}, "User's submissions to this contest", function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/u' + s.user_id + '/C' + s.contest_id);
				});
			}
		}

		old_tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/s/' + submission_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function OldSubmissionsLister(elem, query_suffix /*= ''*/, show_submission /*= function(){return true;}*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';
	if (show_submission === undefined)
		show_submission = function() { return true; };

	this.show_user = (query_suffix.indexOf('/u') === -1 &&
		query_suffix.indexOf('/tS') === -1);

	this.show_contest = (query_suffix.indexOf('/C') === -1 &&
		query_suffix.indexOf('/R') === -1 &&
		query_suffix.indexOf('/P') === -1);

	OldLister.call(this, elem);
	this.query_url = '/api/submissions' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, oldmodal) {
		var thead = this_.elem[0].querySelector('thead');
		if (thead === null) {
			if (data.length === 0) {
				var center = document.createElement('center');
				center.className = 'submissions always_in_view';
				center.innerHTML = '<p>There are no submissions to show...</p>';
				this.elem[0].parentNode.appendChild(center);
				return;
			}

			this.elem[0].innerHTML = '<thead><tr>' +
					'<th>Id</th>' +
					'<th>Lang</th>' +
					(this_.show_user ? '<th class="user">User</th>' : '') +
					'<th class="time">Added</th>' +
					'<th class="problem">Problem</th>' +
					'<th class="status">Status</th>' +
					'<th class="score">Score</th>' +
					'<th class="type">Type</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>';
			add_tz_marker(this_.elem[0].querySelector('thead th.time'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			if (!show_submission(x))
				continue;

			var row = document.createElement('tr');
			var td;
			if (x.type === 'Ignored')
				row.className = 'ignored';

			row.appendChild(elem_with_text('td', x.id));
			row.appendChild(elem_with_text('td', x.language));

			// Username
			if (this_.show_user) {
				if (x.user_id === null)
					row.appendChild(elem_with_text('td', 'System'));
				else if (x.user_first_name === null || x.user_last_name === null)
					row.appendChild(elem_with_text('td', x.user_id));
				else {
					td = document.createElement('td');
					td.appendChild(a_view_button(url_user(x.user_id), x.user_first_name + ' ' + x.user_last_name, undefined,
							view_user.bind(null, true, x.user_id)));
					row.appendChild(td);
				}

			}

			// Submission time
			td = document.createElement('td');
			var avb = a_view_button('/s/' + x.id, x.created_at, undefined,
				view_submission.bind(null, true, x.id));
			avb.setAttribute('datetime', x.created_at);
			td.appendChild(normalize_datetime(avb, false)[0]);
			row.appendChild(td);

			// Problem
			if (x.contest_id == null) { // Not in the contest
				td = document.createElement('td');
				td.appendChild(a_view_button('/p/' + x.problem_id, x.problem_name, undefined,
					view_problem.bind(null, true, x.problem_id)));
			} else {
				td = document.createElement('td');
				// Contest
				if (this_.show_contest) {
					td.appendChild(a_view_button('/c/c' + x.contest_id, x.contest_name,
						undefined, view_contest.bind(null, true, x.contest_id)));
					td.appendChild(document.createTextNode(' / '));
				}
				td.appendChild(a_view_button('/c/r' + x.contest_round_id,
					x.contest_round_name, undefined,
					view_contest_round.bind(null, true, x.contest_round_id)));
				td.appendChild(document.createTextNode(' / '));
				td.appendChild(a_view_button('/c/p' + x.contest_problem_id,
					x.contest_problem_name, undefined,
					view_contest_problem.bind(null, true, x.contest_problem_id)));
			}
			row.appendChild(td);

			// Status
			td = document.createElement('td');
			td.className = 'status ' + x.status.class;
			td.innerText = (x.status.class.lastIndexOf('initial') === -1 ? ''
					: 'Initial: ') + x.status.text;
			row.appendChild(td);

			// Score
			row.appendChild(elem_with_text('td', x.score));

			// Type
			row.appendChild(elem_with_text('td', x.type));

			// Actions
			td = document.createElement('td');
			td.append.apply(td, ActionsToHTML.oldsubmission(x.id, x.actions, x.type));
			row.appendChild(td);

			this_.elem[0].querySelector('tbody').appendChild(row);
		}
	};

	this.fetch_more();
}
function tab_submissions_lister(parent_elem, query_suffix /*= ''*/, show_solutions_tab /* = false*/, filter_normal_instead_of_querying_final /*= false*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff, show_submission) {
		var table = $('<table class="old-submissions"></table>').appendTo(parent_elem);
		new OldSubmissionsLister(table, query_suffix + tab_qsuff, show_submission).monitor_scroll();
	}

	var final_binding = (filter_normal_instead_of_querying_final ?
		retab.bind(null, '', function(s) {
			return (s.type === 'Final' || s.type === 'Initial final');
		}) : retab.bind(null, '/tF'));

	var tabs = [
		'All', retab.bind(null, ''),
		'Final', final_binding,
		'Ignored', retab.bind(null, '/tI')
	];
	if (show_solutions_tab)
		tabs.push('Solutions', retab.bind(null, '/tS'));

	old_tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}

/* ================================ Problems ================================ */
function append_problem_tags(elem, problem_id, problem_tags) {
	elem = $(elem);

	var list_tags = function(hidden) {
		var tags = (hidden ? problem_tags.hidden : problem_tags.public);
		var tbody = $('<tbody>');

		var name_input = $('<input>', {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...'
				autofocus: true,
				required: true
		});

		var add_form = [
			OldForm.field_group('Name', name_input),
			$('<input>', {
				type: 'hidden',
				name: 'hidden',
				value: hidden
			}),
			$('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add tag'
				})
			})
		];

		// Add button
		elem.append($('<center>', {html: [
			$('<table>', {class: 'tags-edit'}).append($('<thead>', {
				html: '<tr>' +
					'<th class="name">Name</th>' +
					'<th class="actions">Actions</th>' +
				'</tr>'
			})).append(tbody),
			a_view_button(undefined, 'Add tag', 'btn', function() {
				oldmodal(ajax_form('Add tag',
					'/api/problem/' + problem_id + '/edit/tags/add_tag',
					add_form, function() {
						show_success_via_oldloader($(this)[0], 'The tag has been added.');
						// Add tag
						var input = add_form[0].children('input');
						tags.push(input.val());
						make_row(input.val()).appendTo(tbody);
						input.val('');
						// Close oldmodal
						var oldmodal = $(this).closest('.oldmodal');
						oldmodal.fadeOut(150, close_oldmodal.bind(null, oldmodal));
					}));

				// Focus tag name
				name_input.focus();
			})
		]}));

		var make_row = function(tag) {
			var name_input = $('<input>', {
				type: 'text',
				name: 'name',
				size: 24,
				value: tag,
				// maxlength: 'TODO...'
				required: true
			});

			var edit_form = [
				OldForm.field_group('Name', name_input),
				$('<input>', {
					type: 'hidden',
					name: 'old_name',
					value: tag
				}),
				$('<input>', {
					type: 'hidden',
					name: 'hidden',
					value: hidden
				}),
				$('<div>', {
					html: $('<input>', {
						class: 'btn blue',
						type: 'submit',
						value: 'Edit'
					})
				})
			];

			var delete_from = $('<form>', {html: [
				$('<input>', {
					type: 'hidden',
					name: 'name',
					value: tag
				}),
				$('<input>', {
					type: 'hidden',
					name: 'hidden',
					value: hidden
				})
			]}).add('<label>', {html: [
				'Are you sure to delete the tag: ',
				$('<label>', {
					class: 'problem-tag' + (hidden ? ' hidden' : ''),
					text: tag
				}),
				'?'
			]});

			var row = $('<tr>', {html: [
				$('<td>', {text: tag}),
				$('<td>', {html: [
					a_view_button(undefined, 'Edit', 'btn-small blue', function() {
						edit_form[0].children('input').val(tag); // If one opened edit, typed something and closed edit, it would appear in the next edit without this change
						oldmodal(ajax_form('Edit tag',
							'/api/problem/' + problem_id + '/edit/tags/edit_tag',
							edit_form, function() {
								show_success_via_oldloader($(this)[0], 'done.');
								// Update tag
								var old_tag = tag;
								tag = edit_form[0].children('input').val();
								tags[tags.indexOf(old_tag)] = tag;
								edit_form[1].children('input').val(tag);

								delete_from.find('label').text(tag);
								delete_from.find('input').val(tag);

								row.children().eq(0).text(tag).css({
									'background-color': '#a3ffa3',
									transition: 'background-color 0s cubic-bezier(0.55, 0.06, 0.68, 0.19)'
								});
								// Without this it does not change to green immediately
								setTimeout(function() {
									row.children().eq(0).css({
										'background-color': 'initial',
										transition: 'background-color 2s cubic-bezier(0.55, 0.06, 0.68, 0.19)'
									});
								}, 50);
								// Close oldmodal
								var oldmodal = $(this).closest('.oldmodal');
								oldmodal.fadeOut(150, close_oldmodal.bind(null, oldmodal));
							}));

						// Focus tag name
						name_input.focus();
					}),
					a_view_button(undefined, 'Delete', 'btn-small red',
						dialogue_oldmodal_request.bind(null, 'Delete tag',
							delete_from, 'Yes, delete it', 'btn-small red',
							'/api/problem/' + problem_id + '/edit/tags/delete_tag',
							function(_, oldloader_parent) {
								show_success_via_oldloader(oldloader_parent[0], 'The tag has been deleted.');
								row.fadeOut(800);
								setTimeout(function() { row.remove(); }, 800);
								// Delete tag
								tags.splice(tags.indexOf(tag), 1);
								// Close oldmodal
								var oldmodal = $(this).closest('.oldmodal');
								oldmodal.fadeOut(250, close_oldmodal.bind(null, oldmodal));
							}, 'No, go back'))
				]})
			]});
			return row;
		};

		for (var x in tags)
			make_row(tags[x]).appendTo(tbody);
	};

	old_tabmenu(default_tabmenu_attacher.bind(elem), [
		'Public', list_tags.bind(null, false),
		'Hidden', list_tags.bind(null, true)
	]);
}
function append_change_problem_statement_form(elem, as_oldmodal, problem_id) {
	$(elem).append(ajax_form('Change statement',
		'/api/problem/' + problem_id + '/change_statement',
		OldForm.field_group("New statement's path", {
			type: 'text',
			name: 'path',
			size: 24,
			placeholder: 'The same as the old statement path'
			// maxlength: 'TODO...'
		}).add(OldForm.field_group('File', {
			type: 'file',
			name: 'statement',
			required: true
		})).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Change statement'
			})
		}), function(resp) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], 'Statement change was scheduled');
				view_job(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/jobs/' + resp;
			}
		}));
}
function change_problem_statement(as_oldmodal, problem_id) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('C') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		append_change_problem_statement_form(this, problem_id);

	}, '/p/' + problem_id + '/change_statement');
}
function edit_problem(as_oldmodal, problem_id, opt_hash) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;

		if (actions.indexOf('E') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var elem = $(this);
		elem.append($('<h1>', {
			class: 'align-center',
			html: [
				'Edit problem ', a_view_button('/p/' + problem_id, problem.name, '',
					view_problem.bind(null, true, problem_id))
			]
		}));

		var tabs = [
			'Edit tags', append_problem_tags.bind(null, elem, problem_id, problem.tags)
		];

		if (problem.actions.indexOf('C') !== -1) {
			tabs.push('Change statement',
				append_change_problem_statement_form.bind(null, elem, as_oldmodal, problem_id));
		}

		old_tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/p/' + problem_id + '/edit' + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function delete_problem(as_oldmodal, problem_id) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('D') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete problem',
			'/api/problem/' + problem_id + '/delete', [
				'You are going to delete the problem ',
				a_view_button('/p/' + problem.id, problem.name,
					undefined, view_problem.bind(null, true, problem.id)),
				' and all submissions to it. As this cannot be undone, you have to confirm this with your password.'
			], 'Delete problem');
	}, '/p/' + problem_id + '/delete');
}
function merge_problem(as_oldmodal, problem_id) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var problem = data[0];
		if (problem.actions.indexOf('M') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		api_request_with_password_to_job(this, 'Merge into another problem',
			'/api/problem/' + problem_id + '/merge_into_another', [
				'The problem ',
				a_view_button('/p/' + problem.id, problem.name,
					undefined, view_problem.bind(null, true, problem.id)),
				' is going to be deleted. All its tags and submissions will be transfered to the target problem. Every contest problem that attaches this problem will be updated to attach the target problem.',
				'<br>',
				'Be aware that problem packages will not be merged - this behaves exactly as problem deletion but first transfers everything that uses this problem to use the target problem.',
				'<br>',
				'As this cannot be undone, you have to confirm this with your password.'
			], 'Merge problem', 'Merging has been scheduled.',
			OldForm.field_group('Target problem ID', {
				type: 'text',
				name: 'target_problem',
				size: 6,
				trim_before_send: true,
			}).add(OldForm.field_group('Rejudge transferred submissions', {
				type: 'checkbox',
				name: 'rejudge_transferred_submissions'
			})));
	}, '/p/' + problem_id + '/merge');
}
function rejudge_problem_submissions(problem_id, problem_name) {
	dialogue_oldmodal_request("Rejudge all problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the problem ',
				a_view_button('/p/' + problem_id, problem_name, undefined,
					view_problem.bind(null, true, problem_id)),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/problem/' + problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function reset_problem_time_limits(as_oldmodal, problem_id) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;
		if (actions.indexOf('L') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Reset problem time limits using model solution', '/api/problem/' + problem_id + '/reset_time_limits',
			$('<p>', {
				style: 'margin: 0 0 12px; text-align: center',
				html: [
					'Do you really want to reset problem ',
					a_view_button('/p/' + problem_id, problem.name, undefined, view_problem.bind(null, true, problem_id)),
					' time limits using the model solution?'
				]
			}).add('<center>', {
				style: 'margin: 12px auto 0',
				html: $('<input>', {
					class: 'btn-small blue',
					type: 'submit',
					value: 'Reset time limits'
				}).add('<a>', {
					class: 'btn-small',
					href: (as_oldmodal ? undefined : '/'),
					text: 'Go back',
					click: function() {
						var oldmodal = $(this).closest('.oldmodal');
						if (oldmodal.length === 0)
							history.back();
						else
							close_oldmodal(oldmodal);
					}
				})
			}), function(resp, oldloader_parent) {
				if (as_oldmodal) {
					show_success_via_oldloader($(this)[0], 'Reseting time limits has been scheduled.');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}
		));
	}, '/p/' + problem_id + '/reset_time_limits');
}
function view_problem(as_oldmodal, problem_id, opt_hash /*= ''*/) {
	old_view_ajax(as_oldmodal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var problem = data[0];
		var actions = problem.actions;

		this.append($('<div>', {
			class: 'header',
			html: $('<h1>', {
					text: problem.name
				}).add('<div>', {
					html: ActionsToHTML.oldproblem(problem, true)
				})
		})).append($('<center>', {
			class: 'always_in_view',
			html: $('<div>', {
				class: 'problem-info',
				html: $('<div>', {
					class: 'visibility',
					html: $('<label>', {text: 'Visibility'}).add('<span>', {
						text: problem.visibility[0].toLowerCase() + problem.visibility.slice(1)
					})
				})
				.add($('<div>', {
					class: 'name',
					html: $('<label>', {text: 'Name'})
				}).append(text_to_safe_html(problem.name)))
				.add($('<div>', {
					class: 'label',
					html: $('<label>', {text: 'Label'})
				}).append(text_to_safe_html(problem.label)))
				.add($('<div>', {
					class: 'tags-row',
					html: $('<label>', {text: 'Tags'})
				}).append($('<div>', {
					class: 'tags',
					html: function() {
						var tags = [];
						for (var i in problem.tags.public)
							tags.push($('<label>', {text: problem.tags.public[i]}));
						for (var i in problem.tags.hidden)
							tags.push($('<label>', {
								class: 'hidden',
								text: problem.tags.hidden[i]
							}));

						return tags;
					}()
				})))
			})
		}));

		if (actions.indexOf('o') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'owner',
					html: [
						$('<label>', {text: 'Owner'}),
						(problem.owner_id === null ? '(Deleted)'
							: a_view_button(url_user(problem.owner_id),
								problem.owner_username, undefined,
								view_user.bind(null, true, problem.owner_id)))
					]
				}));

		if (actions.indexOf('a') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'created_at',
					html: $('<label>', {text: 'Created at'})
				}).append(normalize_datetime($('<span>', {
					datetime: problem.created_at,
					text: problem.created_at
				}), true)));

		var elem = $(this);
		var tabs = [];
		if (actions.indexOf('s') !== -1)
			tabs.push('All submissions', function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/p' + problem_id,
						true);
				});

		if (is_signed_in())
			tabs.push('My submissions', function() {
					elem.append($('<div>'));
					tab_submissions_lister(elem.children().last(), '/p' + problem_id + '/u' + signed_user_id);
				});

		if (actions.indexOf('f') !== -1)
			tabs.push('Simfile', function() {
				timed_hide_show(elem.parents('.oldmodal'));
				elem.append($('<pre>', {
					class: 'simfile',
					style: 'text-align: initial',
					text: problem.simfile
				}));
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				var j_table = $('<table>', {
					class: 'jobs'
				});
				elem.append($('<div>', {
					html: j_table
				}));
				new OldJobsLister(j_table, '/p' + problem_id).monitor_scroll();
			});

		if (actions.indexOf('c') !== -1)
			tabs.push('Attaching contest problems', function() {
				var acp_table = $('<table>', {
					class: 'attaching-contest-problems stripped'
				});
				elem.append($('<div>', {
					html: acp_table
				}));
				new AttachingContestProblemsLister(acp_table, problem_id).monitor_scroll();
			});

		old_tabmenu(default_tabmenu_attacher.bind(elem), tabs);

	}, '/p/' + problem_id + (opt_hash === undefined ? '' : opt_hash), undefined, false);
}
function AttachingContestProblemsLister(elem, problem_id, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/problem/' + problem_id + '/attaching_contest_problems' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, oldmodal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'attaching-contest-problems always_in_view',
					// class: 'attaching-contest-problems',
					html: '<p>There are no contest problems using (attaching) this problem...</p>'
				}));
				remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Contest problem</th>' +
				'</tr></thead><tbody></tbody>');
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.problem_id;

			var row = $('<tr>');
			row.append($('<td>', {
				html: [
					a_view_button('/c/c' + x.contest_id, x.contest_name, undefined,
						view_contest.bind(null, true, x.contest_id)),
					' / ',
					a_view_button('/c/r' + x.round_id, x.round_name, undefined,
						view_contest_round.bind(null, true, x.round_id)),
					' / ',
					a_view_button('/c/p' + x.problem_id, x.problem_name, undefined,
						view_contest_problem.bind(null, true, x.problem_id))
				]
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
/* ================================ Contests ================================ */
function append_create_contest(elem, as_oldmodal) {
	elem.append(ajax_form('Create contest', '/api/contest/create',
		OldForm.field_group("Contest's name", {
			type: 'text',
			name: 'name',
			size: 24,
			// maxlength: 'TODO...',
			trim_before_send: true,
			required: true
		}).add(signed_user_is_admin() ? OldForm.field_group('Make public', {
			type: 'checkbox',
			name: 'public'
		}) : $()).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Create'
			})
		}), function(resp) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], 'Created');
				view_contest(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/c' + resp;
			}
		})
	);
}
function append_clone_contest(elem, as_oldmodal) {
	var source_contest_input = $('<input>', {
		type: 'hidden',
		name: 'source_contest'
	});

	elem.append(ajax_form('Clone contest', '/api/contest/clone',
		OldForm.field_group("Contest's name", {
			type: 'text',
			name: 'name',
			size: 24,
			// maxlength: 'TODO...',
			trim_before_send: true,
		}).add(source_contest_input
		).add(OldForm.field_group("Contest to clone (ID or URL)",
			$('<input>', {
				type: 'text',
				required: true
			}).on('input', function () {
				var val = $(this).val().trim();
				if (!isNaN(val)) {
					source_contest_input.val(val);
				} else {
					// Assume that input is an URL
					var match = val.match(/\/c(\d+)\b/);
					if (match != null)
						source_contest_input.val(match[1]);
					else
						source_contest_input.val(''); // unset on invalid value
				}
			})
		)).add(signed_user_is_admin() ? OldForm.field_group('Make public', {
			type: 'checkbox',
			name: 'public'
		}) : $()).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Clone'
			})
		}), function(resp) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], 'Cloned');
				view_contest(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/c' + resp;
			}
		})
	);
}
function add_contest(as_oldmodal, opt_hash /*= undefined*/) {
	view_base(as_oldmodal, '/c/add' + (opt_hash === undefined ? '' : opt_hash), function() {
		this.append($('<h1>', {
			class: 'align-center',
			text: 'Add contest'
		}));

		old_tabmenu(default_tabmenu_attacher.bind(this), [
			'Create new', append_create_contest.bind(null, this, as_oldmodal),
			'Clone existing', append_clone_contest.bind(null, this, as_oldmodal),
		]);
	});
}
function append_create_contest_round(elem, as_oldmodal, contest_id) {
	elem.append(ajax_form('Create contest round', '/api/contest/c' + contest_id + '/create_round',
		OldForm.field_group("Round's name", {
			type: 'text',
			name: 'name',
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			required: true
		}).add(OldForm.field_group('Begin time',
			dt_chooser_input('begins', true, true, true, undefined, 'The Big Bang', 'Never')
		)).add(OldForm.field_group('End time',
			dt_chooser_input('ends', true, true, true, Infinity, 'The Big Bang', 'Never')
		)).add(OldForm.field_group('Full results time',
			dt_chooser_input('full_results', true, true, true, -Infinity, 'Immediately', 'Never')
		)).add(OldForm.field_group('Show ranking since',
			dt_chooser_input('ranking_expo', true, true, true, -Infinity, 'The Big Bang', "Don't show")
		)).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Create'
			})
		}), function(resp) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], 'Created');
				view_contest_round(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/r' + resp;
			}
		})
	);
}
function append_clone_contest_round(elem, as_oldmodal, contest_id) {
	var source_contest_round_input = $('<input>', {
		type: 'hidden',
		name: 'source_contest_round'
	});

	elem.append(ajax_form('Clone contest round', '/api/contest/c' + contest_id + '/clone_round',
		OldForm.field_group("Round's name", {
			type: 'text',
			name: 'name',
			size: 25,
			// maxlength: 'TODO...',
			trim_before_send: true,
			placeholder: "The same as name of the cloned round"
		}).add(source_contest_round_input
		).add(OldForm.field_group("Contest round to clone (ID or URL)",
			$('<input>', {
				type: 'text',
				required: true
			}).on('input', function () {
				var val = $(this).val().trim();
				if (!isNaN(val)) {
					source_contest_round_input.val(val);
				} else {
					// Assume that input is an URL
					var match = val.match(/\/r(\d+)\b/);
					if (match != null)
						source_contest_round_input.val(match[1]);
					else
						source_contest_round_input.val(''); // unset on invalid value
				}
			})
		)).add(OldForm.field_group('Begin time',
			dt_chooser_input('begins', true, true, true, undefined, 'The Big Bang', 'Never')
		)).add(OldForm.field_group('End time',
			dt_chooser_input('ends', true, true, true, Infinity, 'The Big Bang', 'Never')
		)).add(OldForm.field_group('Full results time',
			dt_chooser_input('full_results', true, true, true, -Infinity, 'Immediately', 'Never')
		)).add(OldForm.field_group('Show ranking since',
			dt_chooser_input('ranking_expo', true, true, true, -Infinity, 'The Big Bang', "Don't show")
		)).add('<div>', {
			html: $('<input>', {
				class: 'btn blue',
				type: 'submit',
				value: 'Clone'
			})
		}), function(resp) {
			if (as_oldmodal) {
				show_success_via_oldloader($(this)[0], 'Cloned');
				view_contest_round(true, resp);
			} else {
				this.parent().remove();
				window.location.href = '/c/r' + resp;
			}
		})
	);
}
function add_contest_round(as_oldmodal, contest_id, opt_hash /*= undefined*/) {
	view_base(as_oldmodal, '/c/c' + contest_id + '/add_round' + (opt_hash === undefined ? '' : opt_hash), function() {
		this.append($('<h1>', {
			class: 'align-center',
			text: 'Add contest round'
		}));

		old_tabmenu(default_tabmenu_attacher.bind(this), [
			'Create new', append_create_contest_round.bind(null, this, as_oldmodal, contest_id),
			'Clone existing', append_clone_contest_round.bind(null, this, as_oldmodal, contest_id),
		]);
	});
}
function add_contest_problem(as_oldmodal, contest_round_id) {
	view_base(as_oldmodal, '/c/r' + contest_round_id + '/attach_problem', function() {
		this.append(ajax_form('Attach problem', '/api/contest/r' + contest_round_id + '/attach_problem',
			OldForm.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as in Problems',
			}).add(OldForm.field_group('Problem ID', {
				type: 'text',
				name: 'problem_id',
				size: 6,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).append(elem_link_to_view('Search problems', list_problems, url_problems))
			).add(OldForm.field_group('Final submission to select',
				$('<select>', {
					name: 'method_of_choosing_final_submission',
					required: true,
					html: $('<option>', {
						value: 'latest_compiling',
						text: 'Latest compiling'
					}).add('<option>', {
						value: 'highest_score',
						text: 'The one with the highest score',
						selected: true
					})
				})
			)).add(OldForm.field_group('Score revealing',
				$('<select>', {
					name: 'score_revealing',
					required: true,
					html: $('<option>', {
						value: 'none',
						text: 'None',
						selected: true
					}).add('<option>', {
						value: 'only_score',
						text: 'Only score'
					}).add('<option>', {
						value: 'score_and_full_status',
						text: 'Score and full status'
					})
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_oldmodal) {
					show_success_via_oldloader($(this)[0], 'Added');
					view_contest_problem(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/c/p' + resp;
				}
			})
		);
	});
}
function edit_contest(as_oldmodal, contest_id) {
	old_view_ajax(as_oldmodal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data.actions;
		if (actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var make_submitters_contestants = OldForm.field_group("Add users who submitted solutions as contestants", {
			type: 'checkbox',
			name: 'make_submitters_contestants',
			checked: true,
			disabled: data.is_public,
		});
		if (data.is_public)
			make_submitters_contestants.hide(0);

		this.append(ajax_form('Edit contest', '/api/contest/c' + contest_id + '/edit',
			OldForm.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				value: data.name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(OldForm.field_group('Public', {
				type: 'checkbox',
				name: 'public',
				checked: data.is_public,
				disabled: (data.is_public || actions.indexOf('M') !== -1 ? undefined : true),
				click: function () {
					make_submitters_contestants.find('input').prop('disabled', this.checked);
					if (this.checked) {
						make_submitters_contestants.hide(0);
					} else {
						make_submitters_contestants.show(0);
					}
				}
			})).add(make_submitters_contestants)
			.add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/c/c' + contest_id + '/edit');
}
function edit_contest_round(as_oldmodal, contest_round_id) {
	old_view_ajax(as_oldmodal, '/api/contest/r' + contest_round_id, function(data) {
		if (data.contest.actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var round = data.rounds[0];
		this.append(ajax_form('Edit round', '/api/contest/r' + contest_round_id + '/edit',
			OldForm.field_group("Round's name", {
				type: 'text',
				name: 'name',
				value: round.name,
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(OldForm.field_group('Begin time',
				dt_chooser_input('begins', true, true, true, utcdt_or_tm_to_Date(round.begins), 'The Big Bang', 'Never')
			)).add(OldForm.field_group('End time',
				dt_chooser_input('ends', true, true, true, utcdt_or_tm_to_Date(round.ends), 'The Big Bang', 'Never')
			)).add(OldForm.field_group('Full results time',
				dt_chooser_input('full_results', true, true, true, utcdt_or_tm_to_Date(round.full_results), 'Immediately', 'Never')
			)).add(OldForm.field_group('Show ranking since',
				dt_chooser_input('ranking_expo', true, true, true, utcdt_or_tm_to_Date(round.ranking_exposure), 'The Big Bang', "Don't show")
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));
	}, '/c/r' + contest_round_id + '/edit');
}
function edit_contest_problem(as_oldmodal, contest_problem_id) {
	old_view_ajax(as_oldmodal, '/api/contest/p' + contest_problem_id, function(data) {
		if (data.contest.actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var problem = data.problems[0];
		this.append(ajax_form('Edit problem', '/api/contest/p' + contest_problem_id + '/edit',
			OldForm.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				value: problem.name,
				size: 25,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).add(OldForm.field_group('Score revealing',
				$('<select>', {
					name: 'score_revealing',
					required: true,
					html: $('<option>', {
						value: 'none',
						text: 'None',
						selected: (problem.score_revealing === 'none')
					}).add('<option>', {
						value: 'only_score',
						text: 'Only score',
						selected: (problem.score_revealing === 'only_score')
					}).add('<option>', {
						value: 'score_and_full_status',
						text: 'Score and full status',
						selected: (problem.score_revealing === 'score_and_full_status')
					})
				})
			)).add(OldForm.field_group("Final submission to select",
				$('<select>', {
					name: 'method_of_choosing_final_submission',
					required: true,
					html: $('<option>', {
						value: 'latest_compiling',
						text: 'Latest compiling',
						selected: (problem.method_of_choosing_final_submission === 'latest_compiling')
					}).add('<option>', {
						value: 'highest_score',
						text: 'The one with the highest score',
						selected: (problem.method_of_choosing_final_submission === 'highest_score')
					})
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			}), function(resp, oldloader_parent) {
				if (resp === '')
					show_success_via_oldloader($(this)[0], 'Updated');
				else if (as_oldmodal) {
					show_success_via_oldloader($(this)[0], 'Updated');
					view_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}
		));
	}, '/c/p' + contest_problem_id + '/edit');
}
function rejudge_contest_problem_submissions(contest_problem_id, contest_problem_name) {
	dialogue_oldmodal_request("Rejudge all contest problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the contest problem ',
				a_view_button('/c/p' + contest_problem_id, contest_problem_name, undefined,
					view_contest_problem.bind(null, true, contest_problem_id)),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/contest/p' + contest_problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function delete_contest(as_oldmodal, contest_id) {
	old_view_ajax(as_oldmodal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_oldloader(this, {
					status: '404',
					statusText: 'Not Found'
				});

		var contest = data[0];
		if (contest.actions.indexOf('D') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest',
			'/api/contest/c' + contest_id + '/delete', [
				'You are going to delete the contest ',
				a_view_button('/c/c' + contest.id, contest.name,
					undefined, view_contest.bind(null, true, contest.id)),
				' all submissions to it and all attached files. As it cannot be undone, you have to confirm it with your password.'
			], 'Delete contest');
	}, '/c/c' + contest_id + '/delete');
}
function delete_contest_round(as_oldmodal, contest_round_id) {
	old_view_ajax(as_oldmodal, '/api/contest/r' + contest_round_id, function(data) {
		var contest = data.contest;
		var round = data.rounds[0];
		if (contest.actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest round',
			'/api/contest/r' + contest_round_id + '/delete', [
					'You are going to delete the contest round ',
					a_view_button('/c/r' + round.id, round.name,
						undefined, view_contest_round.bind(null, true, round.id)),
					' and all submissions to it. As it cannot be undone, you have to confirm it with your password.'
				], 'Delete round');
	}, '/c/r' + contest_round_id + '/delete');
}
function delete_contest_problem(as_oldmodal, contest_problem_id) {
	old_view_ajax(as_oldmodal, '/api/contest/p' + contest_problem_id, function(data) {
		var contest = data.contest;
		var problem = data.problems[0];
		if (contest.actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		delete_with_password_to_job(this, 'Delete contest problem',
			'/api/contest/p' + contest_problem_id + '/delete', [
					'You are going to delete the contest problem ',
					a_view_button('/c/p' + problem.id, problem.name,
						undefined, view_contest_problem.bind(null, true, problem.id)),
					' and all submissions to it. As it cannot be undone, you have to confirm it with your password.'
				], 'Delete contest problem');
	}, '/c/p' + contest_problem_id + '/delete');
}
function view_contest_impl(as_oldmodal, id_for_api, opt_hash /*= ''*/) {
	old_view_ajax(as_oldmodal, '/api/contest/' + id_for_api, function(data) {
		var contest = data.contest;
		var rounds = data.rounds;
		var problems = data.problems;
		var actions = contest.actions;

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b.item - a.item; });

		// Sort problems by (round_id, item)
		problems.sort(function(a, b) {
			return (a.round_id == b.round_id ? a.item - b.item :
				a.round_id - b.round_id);
		});

		// Header
		var header = $('<div>', {class: 'header', html:
			$('<h2>', {
				class: 'contest-path',
				html: function() {
					var res = [
						(id_for_api[0] === 'c' ? $('<span>', {text: contest.name}) :
							$('<a>', {
								href: '/c/c' + contest.id,
								text: contest.name
							}))
					];

					if (id_for_api[0] !== 'c')
						res.push(' / ', (id_for_api[0] === 'r' ?
							$('<span>', {text: rounds[0].name}) :
							$('<a>', {
								href: '/c/r' + rounds[0].id,
								text: rounds[0].name
							})));

					if (id_for_api[0] === 'p') {
						res.push(' / ', $('<span>', {text: problems[0].name}));
						// res.push($('<a>', {
						// 	class: 'btn-small',
						// 	href: '/api/contest/p' + problems[0][0] + '/statement/' +
						// 		encodeURIComponent(problems[0][4]),
						// 	text: 'View statement'
						// }));
					}
					return res;
				}()
		})}).appendTo(this);

		// Contest buttons
		if (id_for_api[0] === 'c')
			header.append($('<div>', {html: [
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest.id + '/add_round', 'Add round', 'btn-small', add_contest_round.bind(null, true, contest.id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/c' + contest.id + '/edit', 'Edit', 'btn-small blue', edit_contest.bind(null, true, contest.id))),
				(actions.indexOf('D') === -1 ? '' : a_view_button('/c/c' + contest.id + '/delete', 'Delete', 'btn-small red', delete_contest.bind(null, true, contest.id))),
			]}));

		// Round buttons
		else if (id_for_api[0] === 'r')
			header.append($('<div>', {html: [
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/attach_problem', 'Attach problem', 'btn-small', add_contest_problem.bind(null, true, rounds[0].id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/edit', 'Edit', 'btn-small blue', edit_contest_round.bind(null, true, rounds[0].id))),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/r' + rounds[0].id + '/delete', 'Delete', 'btn-small red', delete_contest_round.bind(null, true, rounds[0].id)))
			]}));

		// Problem buttons
		else if (id_for_api[0] === 'p')
			header.append($('<div>', {html: [
				(problems[0].can_view_problem ? a_view_button('/p/' + problems[0].problem_id, 'View in Problems', 'btn-small green', view_problem.bind(null, true, problems[0].problem_id)) : ''),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0].id + '/edit', 'Edit', 'btn-small blue', edit_contest_problem.bind(null, true, problems[0].id))),
				(actions.indexOf('A') === -1 ? '' :
					$('<a>', {
						class: 'btn-small blue',
						text: 'Rejudge submissions',
						click: rejudge_contest_problem_submissions.bind(null, problems[0].id, problems[0].name)
					})),
				(actions.indexOf('A') === -1 ? '' : a_view_button('/c/p' + problems[0].id + '/delete', 'Delete', 'btn-small red', delete_contest_problem.bind(null, true, problems[0].id)))
			]}));

		var contest_dashboard;
		var elem = $(this);
		var tabs = [
			'Dashboard', function() {
				timed_hide_show(elem.parents('.oldmodal'));
				// If already generated, then just show it
				if (contest_dashboard !== undefined) {
					contest_dashboard.appendTo(elem);
					return;
				}

				var dashboard = $('<div>', {class: 'contest-dashboard'})
					.appendTo($('<div>', {style: 'margin: 0 16px'}).appendTo(elem));
				contest_dashboard = dashboard.parent();

				var wrap_arrows = function(c, elem) {
					if (id_for_api[0] === c)
						return $('<div>', {
							style: 'position: relative',
							html: [
								elem,
								'<svg class="left" width="16" height="16"><polygon points="0,0 16,8 0,16 4,8"></polygon></svg>',
								'<svg class="right" width="16" height="16"><polygon points="16,0 0,8 16,16 12,8"></polygon></svg>'
							]
						});

					return elem;
				};
				// Contest
				dashboard.append(wrap_arrows('c', $('<a>', {
					class: 'contest',
					href: '/c/c' + contest.id,
					text: contest.name
				})));

				var dashboard_rounds = $('<div>', {
					class: 'rounds'
				}).appendTo(dashboard);

				// Rounds and problems
				function append_round(round) {
					return $('<div>', {
						class: 'round',
						html: [
							wrap_arrows('r', $('<a>', {
								href: '/c/r' + round.id,
								html: function() {
									var res = [$('<span>', {text: round.name})];
									if (rounds.length !== 1)
										res.push($('<div>', {html: [
											$('<label>', {text: "B:"}),
											infdatetime_to($('<span>'), round.begins,
												'The Big Bang', 'never')
										]}),
										$('<div>', {html: [
											$('<label>', {text: "E:"}),
											infdatetime_to($('<span>'), round.ends,
												'The Big Bang', 'never')
										]}),
										$('<div>', {html: [
											$('<label>', {text: "R:"}),
											infdatetime_to($('<span>'), round.full_results,
												'immediately', 'never')
										]}));
									else
										res.push($('<table>', {html: [
											$('<tr>', {html: [
												$('<td>', {text: 'Beginning'}),
												infdatetime_to($('<td>'), round.begins,
													'The Big Bang', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: 'End'}),
												infdatetime_to($('<td>'), round.ends,
													'The Big Bang', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Full results"}),
												infdatetime_to($('<td>'), round.full_results,
													'immediately', 'never')
											]}),
											$('<tr>', {html: [
												$('<td>', {text: "Ranking since"}),
												infdatetime_to($('<td>'), round.ranking_exposure,
													'The Big Bang', 'never')
											]}),
										]}));

									return res;
								}()
							}))
						]
					}).appendTo(dashboard_rounds);
				}

				function append_problem(dashboard_round, problem, cannot_submit) {
					// Problem buttons and statement preview
					if (id_for_api[0] === 'p') {
						var statement_url = '/api/download/statement/contest/p' + problem.id + '/' + encodeURIComponent(problem.name);
						$('<center>', {html: [
							$('<a>', {
								class: 'btn-small',
								href: statement_url,
								text: 'Statement'
							}),
							(cannot_submit ? '' : a_view_button('/c/p' + problem.id + '/submit', 'Submit', 'btn-small blue',
								add_contest_submission.bind(null, true, contest, round, problem))),
						]}).appendTo(dashboard.parent());

						$('<object>').prop({
							class: 'statement',
							data: statement_url
						}).appendTo(dashboard.parent().parent());
					}

					var elem = $('<a>', {
						href: '/c/p' + problem.id,
						style: (id_for_api[0] === 'p' ?
							'padding: 7px; line-height: 12px' : undefined),
						class: (problem.color_class === null ? undefined : 'status ' + problem.color_class),
						html: [
							$('<span>', {text: problem.name}),
							(id_for_api[0] === 'p' ? $('<table>', {html: [
								$('<tr>', {html: [
									$('<td>', {text: 'Final submission'}),
									$('<td>', {text: problem.method_of_choosing_final_submission === 'latest_compiling' ?
										'Latest compiling' : 'One with the highest score'})
								]}),
								(problem.score_revealing !== 'none' ? $('<tr>', {html: [
									$('<td>', {text: 'Score revealing'}),
									$('<td>', {text: problem.score_revealing === 'only_score' ? 'Only score' : 'Score and full status'})
								]}) : $())
							]}) : $())
						]
					});

					wrap_arrows('p', elem).appendTo(dashboard_round);
				}

				for (var i = 0; i < rounds.length; ++i) {
					var round = rounds[i];
					var dashboard_round = append_round(round);
					// Round's problems
					// Bin-search first problem
					var l = 0, r = problems.length;
					while (l < r) {
						var mid = (l + r) >> 1;
						var p = problems[mid];
						if (p.round_id < round.id)
							l = mid + 1;
						else
							r = mid;
					}
					// Check whether the round has ended
					var cannot_submit = (actions.indexOf('p') === -1);
					if (actions.indexOf('A') === -1) {
						var curr_time = date_to_datetime_str(new Date());
						var end_time = utcdt_or_tm_to_Date(round.ends);
						cannot_submit |= (end_time <= curr_time);
					}
					// Append problems
					while (l < problems.length && problems[l].round_id == round.id)
						append_problem(dashboard_round, problems[l++], cannot_submit);
				}

				if (is_signed_in()) {
					// All problems have been colored
					// Mark rounds as green (only the ones that contain a problem)
					var rounds_to_check = dashboard_rounds.children().filter(function() { return $(this).children().length > 1; });
					var green_contest = rounds_to_check.length > 0;

					rounds_to_check.each(function() {
						var round = $(this);
						var round_a = round.find('a').eq(0);
						if (round_a.hasClass('green'))
							return;

						var problems = round.find('a').slice(1);
						if (problems.filter(function() {
								return $(this).hasClass('green');
							}).length === problems.length)
						{
							round_a.addClass('status green');
						} else
							green_contest = false;
					});

					if (green_contest)
						dashboard.find('.contest').addClass('status green');
				}
			}
		];

		if (actions.indexOf('A') !== -1)
			tabs.push('All submissions', function() {
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase());
			});

		if (actions.indexOf('p') !== -1)
			tabs.push('My submissions', function() {
				tab_submissions_lister($('<div>').appendTo(elem), '/' + id_for_api.toUpperCase() + '/u' + signed_user_id, false, (actions.indexOf('A') === -1));
			});

		if (actions.indexOf('v') !== -1)
			tabs.push('Ranking', function() {
				contest_ranking($('<div>').appendTo(elem), id_for_api);
			});

		if (actions.indexOf('A') !== -1)
			tabs.push('Users', function() {
				var entry_link_elem = $('<div>').appendTo(elem);
				var entry_token_elem = $('<div>').appendTo(entry_link_elem);
				var entry_short_token_elem = $('<div>').appendTo(entry_link_elem);
				API_get(url_api_contest_entry_tokens_view(contest.id), function(data) {
					let show_no_short_token;
					if (data.token != null) {
						let show_token;
						const show_no_token = (token) => {
							remove_children(entry_token_elem[0]);
							if (token.capabilities.create) {
								entry_token_elem.append(a_view_button(undefined, 'Add entry link', 'btn', oldmodal_request.bind(null,
									'Add entry link', $('<form>'), url_api_contest_entry_tokens_add(contest.id),
									function(resp, oldloader_parent) {
										show_token(resp.token);
										show_no_short_token(data.short_token);
										show_success_via_oldloader(oldloader_parent[0], 'Added');
									})))
							}
						};
						show_token = (token) => {
							remove_children(entry_token_elem[0]);
							entry_token_elem.append(copy_to_clipboard_btn(false, 'Copy link', function() {
								return window.location.origin + url_enter_contest(token.value);
							}));
							if (token.capabilities.regenerate) {
								entry_token_elem.append(a_view_button(undefined, 'Regenerate link', 'btn blue', dialogue_oldmodal_request.bind(null,
									'Regenerate link', $('<center>', {
										html: [
											'Are you sure to regenerate the entry link: ',
											$('<br>'),
											a_view_button(url_enter_contest(token.value), window.location.origin + url_enter_contest(token.value), undefined, enter_contest_using_token.bind(null, true, token.value)),
											$('<br>'),
											'?'
										],
									}), 'Yes, regenerate it', 'btn-small blue', url_api_contest_entry_tokens_regenerate(contest.id),
									function (resp, oldloader_parent) {
										show_token(resp.token);
										show_success_via_oldloader(oldloader_parent[0], 'Regenerated');
									}, 'No, take me back', true)));
							}
							if (token.capabilities.delete) {
								entry_token_elem.append(a_view_button(undefined, 'Delete link', 'btn red', dialogue_oldmodal_request.bind(null,
									'Delete link', $('<center>', {
										html: [
											'Are you sure to delete the entry link: ',
											$('<br>'),
											a_view_button(url_enter_contest(token.value), window.location.origin + url_enter_contest(token.value), undefined, enter_contest_using_token.bind(null, true, token.value)),
											$('<br>'),
											'?'
										],
									}), 'Yes, I am sure', 'btn-small red', url_api_contest_entry_tokens_delete(contest.id),
									function (resp, oldloader_parent) {
										show_no_token(token);
										remove_children(entry_short_token_elem[0]);
										show_success_via_oldloader(oldloader_parent[0], 'Deleted');
									}, 'No, take me back', true)));
							}

							entry_token_elem.append($('<pre>', {
								text: window.location.origin + url_enter_contest(token.value),
							}));
						};

						if (data.token.value === null) {
							show_no_token(data.token);
						} else {
							show_token(data.token);
						}
					}
					if (data.short_token != null) {
						let show_short_token;
						show_no_short_token = (short_token) => {
							remove_children(entry_short_token_elem[0]);
							entry_short_token_elem.append(a_view_button(undefined, 'Add short entry link', 'btn', oldmodal_request.bind(null,
								'Add short entry link', $('<form>'), url_api_contest_entry_tokens_add_short(contest.id),
								function(resp, oldloader_parent) {
									show_short_token(resp.short_token);
									show_success_via_oldloader(oldloader_parent[0], 'Added');
								})));
						};
						show_short_token = (short_token) => {
							remove_children(entry_short_token_elem[0]);
							entry_short_token_elem.append(copy_to_clipboard_btn(false, 'Copy short link', function() {
								return window.location.origin + url_enter_contest(short_token.value);
							}));
							if (short_token.capabilities.regenerate) {
								entry_short_token_elem.append(a_view_button(undefined, 'Regenerate short link', 'btn blue', dialogue_oldmodal_request.bind(null,
									'Regenerate short link', $('<center>', {
										html: [
											'Are you sure to regenerate the entry short link: ',
											$('<br>'),
											a_view_button(url_enter_contest(short_token.value), window.location.origin + url_enter_contest(short_token.value), undefined, enter_contest_using_token.bind(null, true, short_token.value)),
											$('<br>'),
											'?'
										]
									}), 'Yes, regenerate it', 'btn-small blue', url_api_contest_entry_tokens_regenerate_short(contest.id),
									function(resp, oldloader_parent) {
										show_short_token(resp.short_token);
										show_success_via_oldloader(oldloader_parent[0], 'Regenerated');
									}, 'No, take me back', true)));
							}
							if (short_token.capabilities.delete) {
								entry_short_token_elem.append(a_view_button(undefined, 'Delete short link', 'btn red', dialogue_oldmodal_request.bind(null,
									'Delete short link', $('<center>', {
										html: [
											'Are you sure to delete the entry short link: ',
											$('<br>'),
											a_view_button(url_enter_contest(short_token.value), window.location.origin + url_enter_contest(short_token.value), undefined, enter_contest_using_token.bind(null, true, short_token.value)),
											$('<br>'),
											'?'
										]
									}), 'Yes, I am sure', 'btn-small red', url_api_contest_entry_tokens_delete_short(contest.id),
									function(resp, oldloader_parent) {
										show_no_short_token(short_token);
										show_success_via_oldloader(oldloader_parent[0], 'Deleted');
									}, 'No, take me back', true)));
							}

							const exp_date = new Date(short_token.expires_at);
							entry_short_token_elem.append($('<span>', {
								style: 'margin-left: 10px; font-size: 12px; color: #777;',
								html: ['Short token will expire in ', countdown_clock(exp_date)],
							}));
							entry_short_token_elem.append($('<pre>', {
								text: window.location.origin + url_enter_contest(short_token.value),
							}));
						};
						if (data.token.value != null) {
							if (data.short_token.value === null) {
								show_no_short_token(data.short_token);
							} else {
								show_short_token(data.short_token);
							}
						}
					}
				}, entry_link_elem);

				tab_contest_users_lister($('<div>').appendTo(elem), '/c' + contest.id);
			});

		tabs.push('Files', function() {
			var table = $('<table class="contest-files stripped"></table>').appendTo($('<div>').appendTo(elem));
			new ContestFilesLister(table, '/c' + contest.id).monitor_scroll();
		});

		var header_links_updater = function(_, active_elem) {
			// Add / replace hashes in links in the contest-path
			elem.find('.contest-path').children('a:not(.btn-small)').each(function() {
				var xx = $(this);
				var href = xx.attr('href');
				var pos = href.indexOf('#');
				if (pos !== -1)
					href = href.substring(0, pos);
				xx.attr('href', href + window.location.hash);
			});
		};

		old_tabmenu(tabmenu_attacher_with_change_callback.bind(elem, header_links_updater), tabs);

	}, '/c/' + id_for_api + (opt_hash === undefined ? '' : opt_hash));
}
function view_contest(as_oldmodal, contest_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_oldmodal, 'c' + contest_id, opt_hash);
}
function view_contest_round(as_oldmodal, contest_round_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_oldmodal, 'r' + contest_round_id, opt_hash);
}
function view_contest_problem(as_oldmodal, contest_problem_id, opt_hash /*= ''*/) {
	return view_contest_impl(as_oldmodal, 'p' + contest_problem_id, opt_hash);
}
function contest_ranking(elem_, id_for_api) {
	var elem = elem_;
	old_API_call('/api/contest/' + id_for_api, function(cdata) {
		var contest = cdata.contest;
		var rounds = cdata.rounds;
		var problems = cdata.problems;
		var is_admin = (contest.actions.indexOf('A') !== -1);

		// Sort rounds by -items
		rounds.sort(function(a, b) { return b.item - a.item; });
		// Map rounds (by id) to their items
		var rid_to_item = new StaticMap();
		var curr_time = new Date();
		for (var i = 0; i < rounds.length; ++i) {
			if (!is_admin) {
				// Add only those rounds that have the ranking_exposure point has passed
				var ranking_exposure = utcdt_or_tm_to_Date(rounds[i].ranking_exposure);
				if (curr_time < ranking_exposure)
					continue; // Do not show the round
			}

			// Show the round
			rid_to_item.add(rounds[i].id, rounds[i].item);
		}
		rid_to_item.prepare();

		// Add round item to the every problem (and remove invalid problems -
		// the ones which don't belong to the valid rounds)
		var tmp_problems = [];
		for (var i = 0; i < problems.length; ++i) {
			var x = rid_to_item.get(problems[i].round_id);
			if (x != null) {
				problems[i].round_item = x;
				tmp_problems.push(problems[i]);
			}
		}
		problems = tmp_problems;

		// Sort problems by (-round_item, item)
		problems.sort(function(a, b) {
			return (a.round_item == b.round_item ? a.item - b.item :
				b.round_item - a.round_item);
		});

		// Map problems (by id) to their indexes in the above array
		var problem_to_col_id = new StaticMap();
		for (var i = 0; i < problems.length; ++i)
			problem_to_col_id.add(problems[i].id, i);
		problem_to_col_id.prepare();

		old_API_call('/api/contest/' + id_for_api + '/ranking', function(data) {
			var oldmodal = elem.parents('.oldmodal');
			if (data.length == 0) {
				timed_hide_show(oldmodal);
				var message_to_show = '<p>There is no one in the ranking yet...</p>';
				if (rounds.length === 1) {
					var ranking_exposure = utcdt_or_tm_to_Date(rounds[0].ranking_exposure);
					if (ranking_exposure === Infinity)
						message_to_show = '<p>The current round will not be shown in the ranking.</p>';
					else if (ranking_exposure > new Date())
						message_to_show = $('<p>Ranking will be available since: </p>').append(normalize_datetime($('<span>', {datetime: rounds[0].ranking_exposure})));
				}

				return elem.append($('<center>', {
					class: 'always_in_view',
					html: message_to_show
				}));
			}

			// Construct table's head
			var tr = $('<tr>', {
				html: [
					$('<th>', {
						rowspan: 2,
						text: '#'
					}),
					$('<th>', {
						rowspan: 2,
						text: 'User'
					}),
					$('<th>', {
						rowspan: 2,
						text: 'Sum'
					}),
				]
			});
			// Add rounds
			var colspan = 0;
			var j = 0; // Index of the current round
			for (var i = 0; i < problems.length; ++i) {
				var problem = problems[i];
				while (rounds[j].id != problem.round_id)
					++j; // Skip rounds (that have no problems attached)

				++colspan;
				// If current round's problems end
				if (i + 1 == problems.length || problems[i + 1].round_id != problem.round_id) {
					tr.append($('<th>', {
						colspan: colspan,
						html: $('<a>', {
							href: '/c/r' + rounds[j].id + '#ranking',
							text: rounds[j].name
						})
					}));
					colspan = 0;
					++j;
				}
			}
			var thead = $('<thead>', {html: tr});
			tr = $('<tr>');
			// Add problems
			for (var i = 0; i < problems.length; ++i)
				tr.append($('<th>', {
					html: $('<a>', {
						href: '/c/p' + problems[i].id + '#ranking',
						text: problems[i].problem_label
					})
				}));
			thead.append(tr);

			// Add score for each user add this to the user's info
			var submissions;
			for (var i = 0; i < data.length; ++i) {
				submissions = data[i].submissions;
				var total_score = 0;
				// Count only valid problems (to fix potential discrepancies
				// between ranking submissions and the contest structure)
				for (var j = 0; j < submissions.length; ++j) {
					if (problem_to_col_id.get(submissions[j].contest_problem_id) !== null) {
						total_score += submissions[j].score;
					}
				}

				data[i].score = total_score;
			}

			// Sort users (and their submissions) by their -score
			data.sort(function(a, b) { return b.score - a.score; });

			// Add rows
			var tbody = $('<tbody>');
			var prev_score = data[0].score + 1;
			var place;
			for (var i = 0; i < data.length; ++i) {
				var user_row = data[i];
				tr = $('<tr>');
				// Place
				if (prev_score != user_row.score) {
					place = i + 1;
					prev_score = user_row.score;
				}
				tr.append($('<td>', {text: place}));
				// User
				if (user_row.id === null)
					tr.append($('<td>', {text: user_row.name}));
				else {
					tr.append($('<td>', {
						html: a_view_button(url_user(user_row.id), user_row.name, '',
							view_user.bind(null, true, user_row.id))
					}));
				}
				// Score
				tr.append($('<td>', {text: user_row.score}));
				// Submissions
				var row = new Array(problems.length);
				submissions = data[i].submissions;
				for (var j = 0; j < submissions.length; ++j) {
					var x = problem_to_col_id.get(submissions[j].contest_problem_id);
					if (x != null) {
						var score_text = (submissions[j].score != null ? submissions[j].score : '?');
						if (submissions[j].id === null) {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j].status.class,
								text: score_text
							});
						} else {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j].status.class,
								html: a_view_button('/s/' + submissions[j].id,
									score_text, '',
									view_submission.bind(null, true, submissions[j].id))
							});
						}
					}
				}
				// Construct the row
				for (var j = 0; j < problems.length; ++j) {
					if (row[j] === undefined)
						$('<td>').appendTo(tr);
					else
						row[j].appendTo(tr);
				}

				tbody.append(tr);
			}

			elem.append($('<table>', {
				class: 'table ranking stripped',
				html: [thead, tbody]
			}));

			timed_hide_show(oldmodal);
			centerize_oldmodal(oldmodal, false);

		}, elem);
	}, elem);
}
function ContestsLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contests' + query_suffix;
	this.query_suffix = '';

	this.process_api_response = function(data, oldmodal) {
		if (this_.elem.children('thead').length === 0) {
			if (data.length === 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contests always_in_view',
					// class: 'contests',
					html: '<p>There are no contests to show...</p>'
				}));
				remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					(signed_user_is_admin() ? '<th>Id</th>' : '') +
					'<th class="name">Name</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem.find('thead th.added'));
		}

		for (var x in data) {
			x = data[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>',	{
				class: (x.is_public ? undefined : 'grayed')
			});

			// Id
			if (signed_user_is_admin())
				row.append($('<td>', {text: x.id}));
			// Name
			row.append($('<td>', {
				html: $('<a>', {
					href: '/c/c' + x.id,
					text: x.name
				})
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest(x.id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_contests_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="contests"></table>').appendTo(parent_elem);
		new ContestsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, '')
	];

	// TODO: implement it
	// if (is_signed_in())
		// tabs.push('My', retab.bind(null, '/u' + signed_user_id));

	old_tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function contest_chooser(as_oldmodal /*= true*/, opt_hash /*= ''*/) {
	view_base((as_oldmodal === undefined ? true : as_oldmodal),
		'/c' + (opt_hash === undefined ? '' : opt_hash), function() {
			timed_hide($(this).parent().parent().filter('.oldmodal'));
			$(this).append($('<h1>', {text: 'Contests'}));
			if (signed_user_is_teacher_or_admin())
				$(this).append(a_view_button('/c/add', 'Add contest', 'btn',
					add_contest.bind(null, true)));

			tab_contests_lister($(this));
		});
}

/* ============================ Contest's users ============================ */
function ContestUsersLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contest_users' + query_suffix;
	this.query_suffix = '';

	var x = query_suffix.indexOf('/c');
	var y = query_suffix.indexOf('/', x + 2);
	this.contest_id = query_suffix.substring(x + 2, y == -1 ? undefined : y);

	this.process_api_response = function(data, oldmodal) {
		if (this_.elem.children('thead').length === 0) {
			// Overall actions
			elem.prev('.old_tabmenu').prevAll().remove();
			this_.elem.parent().prepend(ActionsToHTML.contest_users(this_.contest_id, data.overall_actions));

			if (data.rows.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contest-users always_in_view',
					// class: 'contest-users',
					html: '<p>There are no contest users to show...</p>'
				}));
				remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					'<th>Id</th>' +
					'<th class="username">Username</th>' +
					'<th class="first-name">First name</th>' +
					'<th class="last-name">Last name</th>' +
					'<th class="mode">Mode</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
		}

		for (var x in data.rows) {
			x = data.rows[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');
			row.append($('<td>', {text: x.id}));
			row.append($('<td>', {html: a_view_button(url_user(x.id), x.username,
				'', view_user.bind(null, true, x.id))}));
				// x.username}));
			row.append($('<td>', {text: x.first_name}));
			row.append($('<td>', {text: x.last_name}));
			row.append($('<td>', {
				class: x.mode[0].toLowerCase() + x.mode.slice(1),
				text: x.mode
			}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest_user(x.id, this_.contest_id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function tab_contest_users_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		var table = $('<table class="contest-users stripped"></table>').appendTo(parent_elem);
		new ContestUsersLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', retab.bind(null, ''),
		'Owners', retab.bind(null, '/mO'),
		'Moderators', retab.bind(null, '/mM'),
		'Contestants', retab.bind(null, '/mC')
	];

	old_tabmenu(default_tabmenu_attacher.bind(parent_elem), tabs);
}
function add_contest_user(as_oldmodal, contest_id) {
	old_view_ajax(as_oldmodal, '/api/contest_users/c' + contest_id + '/<0', function(data) {
		if (data.overall_actions === undefined)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		var actions = data.overall_actions;
		var add_contestant = (actions.indexOf('Ac') !== -1);
		var add_moderator = (actions.indexOf('Am') !== -1);
		var add_owner = (actions.indexOf('Ao') !== -1);
		if (add_contestant + add_moderator + add_owner < 1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Add contest user', '/api/contest_user/c' + contest_id + '/add',
			OldForm.field_group('User ID', {
				type: 'text',
				name: 'user_id',
				size: 6,
				// maxlength: 'TODO...',
				trim_before_send: true,
				required: true
			}).append(elem_link_to_view('Search users', list_users, url_users))
			.add(OldForm.field_group('Mode', $('<select>', {
				name: 'mode',
				html: function() {
					var res = [];
					if (add_contestant)
						res.push($('<option>', {
							value: 'C',
							text: 'Contestant',
							selected: (data.mode === 'Contestant' ? true : undefined)
						}));

					if (add_moderator)
						res.push($('<option>', {
							value: 'M',
							text: 'Moderator',
							selected: (data.mode === 'Moderator' ? true : undefined)
						}));

					if (add_owner)
						res.push($('<option>', {
							value: 'O',
							text: 'Owner',
							selected: (data.mode === 'Owner' ? true : undefined)
						}));

					return res;
				}()
			}))).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			})
		));

	}, '/c/c' + contest_id + '/contest_user/add');
}
function change_contest_user_mode(as_oldmodal, contest_id, user_id) {
	old_view_ajax(as_oldmodal, '/api/contest_users/c' + contest_id + '/=' + user_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});


		data = data.rows[0];

		var actions = data.actions;
		var make_contestant = (actions.indexOf('Mc') !== -1);
		var make_moderator = (actions.indexOf('Mm') !== -1);
		var make_owner = (actions.indexOf('Mo') !== -1);
		if (make_contestant + make_moderator + make_owner < 2)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Change mode', '/api/contest_user/c' + contest_id + '/u' + user_id + '/change_mode', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Mode of the user ',
						a_view_button(url_user(user_id), data.username, '', view_user.bind(null, true, user_id)),
						': ',
						$('<select>', {
							style: 'margin-left: 4px',
							name: 'mode',
							html: function() {
								var res = [];
								if (make_contestant)
									res.push($('<option>', {
										value: 'C',
										text: 'Contestant',
										selected: (data.mode === 'Contestant' ? true : undefined)
									}));

								if (make_moderator)
									res.push($('<option>', {
										value: 'M',
										text: 'Moderator',
										selected: (data.mode === 'Moderator' ? true : undefined)
									}));

								if (make_owner)
									res.push($('<option>', {
										value: 'O',
										text: 'Owner',
										selected: (data.mode === 'Owner' ? true : undefined)
									}));

								return res;
							}()
						})
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html:
						$('<input>', {
							class: 'btn-small blue',
							type: 'submit',
							value: 'Update'
						})
				})
			]
		})));

	}, '/c/c' + contest_id + '/contest_user/' + user_id + '/change_mode');
}
function expel_contest_user(as_oldmodal, contest_id, user_id) {
	old_view_ajax(as_oldmodal, '/api/contest_users/c' + contest_id + '/=' + user_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});


		data = data.rows[0];
		var actions = data.actions;

		if (actions.indexOf('E') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Expel user from the contest', '/api/contest_user/c' + contest_id + '/u' + user_id + '/expel', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Are you sure to expel the user ',
						a_view_button(url_user(user_id), data.username,
							undefined, view_user.bind(null, true, user_id)),
						'?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small red',
							type: 'submit',
							value: 'Of course!'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'Nah, spare the user',
							click: function() {
								var oldmodal = $(this).closest('.oldmodal');
								if (oldmodal.length === 0)
									history.back();
								else
									close_oldmodal(oldmodal);
							}
						})
					]
				})
			]
		})));

	}, '/c/c' + contest_id + '/contest_user/' + user_id + '/expel');
}

/* ============================= Contest files ============================= */
function ContestFilesLister(elem, query_suffix /*= ''*/) {
	var this_ = this;
	if (query_suffix === undefined)
		query_suffix = '';

	OldLister.call(this, elem);
	this.query_url = '/api/contest_files' + query_suffix;
	this.query_suffix = '';

	var x = query_suffix.indexOf('/c');
	var y = query_suffix.indexOf('/', x + 2);
	this.contest_id = query_suffix.substring(x + 2, y == -1 ? undefined : y);

	this.process_api_response = function(data, oldmodal) {
		var show_creator = (data.overall_actions.indexOf('C') !== -1);
		if (this_.elem.children('thead').length === 0) {
			// Overall actions
			elem.prev('.old_tabmenu').prevAll().remove();
			this_.elem.parent().prepend(ActionsToHTML.contest_files(this_.contest_id, data.overall_actions));

			if (data.rows.length == 0) {
				this_.elem.parent().append($('<center>', {
					class: 'contest-files always_in_view',
					html: '<p>There are no files to show...</p>'
				}));
				remove_oldloader(this_.elem.parent()[0]);
				timed_hide_show(oldmodal);
				return;
			}

			this_.elem.html('<thead><tr>' +
					(show_creator ? '<th class="creator">Creator</th>' : '') +
					'<th class="modified">Modified</th>' +
					'<th class="name">File name</th>' +
					'<th class="size">File size</th>' +
					'<th class="description">Description</th>' +
					'<th class="actions">Actions</th>' +
				'</tr></thead><tbody></tbody>');
			add_tz_marker(this_.elem[0].querySelector('thead th.modified'));
		}

		for (var x in data.rows) {
			x = data.rows[x];
			this_.query_suffix = '/<' + x.id;

			var row = $('<tr>');

			if (show_creator) {
				if (x.creator_id === null)
					row.append($('<td>', {text: '(deleted)'}));
				else
					row.append($('<td>', {html: a_view_button(url_user(x.creator_id),
						x.creator_username, '',
						view_user.bind(null, true, x.creator_id))}));
			}

			row.append(normalize_datetime($('<td>', {
				datetime: x.modified,
				text: x.modified
			})));
			row.append($('<td>', {html: $('<a>', {
				href: '/api/download/contest_file/' + x.id,
				text: x.name
			})}));
			row.append($('<td>', {text: humanize_file_size(x.file_size)}));

			row.append($('<td>', {text: x.description}));

			// Actions
			row.append($('<td>', {
				html: ActionsToHTML.contest_file(x.id, x.actions)
			}));

			this_.elem.children('tbody').append(row);
		}
	};

	this.fetch_more();
}
function add_contest_file(as_oldmodal, contest_id) {
	old_view_ajax(as_oldmodal, '/api/contest_files/c' + contest_id, function(data) {
		if (data.overall_actions === undefined)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		if (data.overall_actions.indexOf('A') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Add contest file',
			'/api/contest_file/add/c' + contest_id, OldForm.field_group('File name', {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as name of the uploaded file',
			}).add(OldForm.field_group('File', {
				type: 'file',
				name: 'file',
				required: true
			})).add(OldForm.field_group('Description',
				$('<textarea>', {
					name: 'description',
					maxlength: 512
				})
			)).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			})
		));

	}, '/c/c' + contest_id + '/files/add');
}
function edit_contest_file(as_oldmodal, contest_file_id) {
	old_view_ajax(as_oldmodal, '/api/contest_files/=' + contest_file_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		file = data.rows[0];
		if (file.actions.indexOf('E') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit contest file',
			'/api/contest_file/' + contest_file_id + '/edit', OldForm.field_group('File name', {
				type: 'text',
				name: 'name',
				value: file.name,
				size: 24,
				// maxlength: 'TODO...',
				trim_before_send: true,
				placeholder: 'The same as name of uploaded file',
			}).add(OldForm.field_group('File', {
				type: 'file',
				name: 'file'
			})).add(OldForm.field_group('Description',
				$('<textarea>', {
					name: 'description',
					maxlength: 512,
					text: file.description
				})
			)).add(OldForm.field_group('File size', {
				type: 'text',
				value: humanize_file_size(file.file_size) + ' (' + file.file_size + ' B)',
				disabled: true
			})).add(OldForm.field_group('Modified', normalize_datetime($('<span>', {
				datetime: file.modified,
				text: file.modified
			}), true))).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/contest_file/' + contest_file_id + '/edit');
}
function delete_contest_file(as_oldmodal, contest_file_id) {
	old_view_ajax(as_oldmodal, '/api/contest_files/=' + contest_file_id, function(data) {
		if (data.rows === undefined || data.rows.length === 0)
			return show_error_via_oldloader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		file = data.rows[0];
		if (file.actions.indexOf('D') === -1)
			return show_error_via_oldloader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Delete contest file',
			'/api/contest_file/' + contest_file_id + '/delete', $('<center>', {
			html: [
				$('<label>', {
					html: [
						'Do you really want to delete the contest file ',
						a_view_button('/contest_file/' + contest_file_id + '/edit', file.name,
							undefined, edit_contest_file.bind(null, true, contest_file_id)),
						'?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small red',
							type: 'submit',
							value: 'Yes, delete it'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'No, take me back',
							click: function() {
								var oldmodal = $(this).closest('.oldmodal');
								if (oldmodal.length === 0)
									history.back();
								else
									close_oldmodal(oldmodal);
							}
						})
					]
				})
			]
		})));

	}, '/contest_file/' + contest_file_id + '/delete');
}

/* ============================ Contest's users ============================ */
function enter_contest_using_token(as_oldmodal, contest_entry_token) {
	view_ajax(as_oldmodal, url_api_contest_name_for_contest_entry_token(contest_entry_token), function(data) {
		this.append(ajax_form('Contest entry', url_api_use_contest_entry_token(contest_entry_token), $('<center>', {
			html: [
				$('<label>', {
					html: [
						'You are about to enter the contest ',
						a_view_button('/c/' + data.contest.id, data.contest.name,
							undefined, view_contest.bind(null, true, data.contest.id)),
						'. Are you sure?'
					]
				}),
				$('<div>', {
					style: 'margin-top: 12px',
					html: [
						$('<input>', {
							class: 'btn-small green',
							type: 'submit',
							value: 'Yes, I want to enter'
						}),
						$('<a>', {
							class: 'btn-small',
							text: 'No, take me back',
							click: function() {
								var oldmodal = $(this).closest('.oldmodal');
								if (oldmodal.length === 0)
									history.back();
								else
									close_oldmodal(oldmodal);
							}
						})
					]
				})
			]
		})));

	}, url_enter_contest(contest_entry_token));
}

function open_calendar_on(time, text_input, hidden_input) {
	var months = ['January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October', 'November', 'December'];
	var month_chooser = [];

	var header = $('<span>', {
		style: 'cursor: pointer; user-select: none',
		click: function() {
			var update_month_and_year = function() {
				var row = month_chooser.find('tr').eq(1).children();
				row.eq(0).text(months[time.getMonth()]);
				row.eq(1).text(time.getFullYear());
			};

			month_chooser = oldmodal($('<table>', {
				class: 'chooser',
				style: 'margin-left: -10px',
				html: $('<tbody>', {
					html: $('<tr>', {
						html: $('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(+1);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="60,21 0,21 30,7"></polygon></svg>'
							})
						}).add('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(+12);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="60,21 0,21 30,7"></polygon></svg>'
							})
						})
					}).add('<tr>', {
						html: $('<td>', {
							style: 'min-width: 170px',
							text: months[time.getMonth()]
						}).add('<td>', {
							text: time.getFullYear()
						})
					}).add('<tr>', {
						html: $('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(-1);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="0,9 60,9 30,23"></polygon></svg>'
							})
						}).add('<td>', {
							html: $('<a>', {
								click: function() {
									change_month(-12);
									update_month_and_year();
								},
								html: '<svg width="60" height="30"><polygon points="0,9 60,9 30,23"></polygon></svg>'
							})
						})
					})
				})
			}), function(oldmodal) {
				oldmodal[0].onoldmodalclose = update_calendar.bind(null);
			});
			month_chooser.find('td:first-child').on('wheel', function(e) {
				e.preventDefault();
				if (e.originalEvent.deltaY > 0)
					change_month(-1);
				else if (e.originalEvent.deltaY < 0)
					change_month(+1);
				update_month_and_year();
			});
			month_chooser.find('td:last-child').on('wheel', function(e) {
				e.preventDefault();
				if (e.originalEvent.deltaY > 0)
					change_month(-12);
				else if (e.originalEvent.deltaY < 0)
					change_month(+12);
				update_month_and_year();
			});
		}
	});

	var change_month = function(diff_num) {
		// Prevent changing to one month ahead of the intended one
		var day_set = time.getDate();
		time.setDate(10);
		time.setMonth(time.getMonth() + diff_num);
		var month = time.getMonth();
		time.setDate(day_set);
		if (time.getMonth() !== month)
			time.setDate(0); // The day no. is too big, so choose the last day of the month
	};

	var calendar = $('<table>', {
		class: 'calendar',
		html: $('<thead>', {
			html: [
				$('<tr>', {
					html: [
						$('<th>', {
							html: $('<a>', {click: function() {
								change_month(-1);
								update_calendar();
							}, html: '<svg width="12" height="14"><polygon points="12,0 12,14 0,7"></polygon></svg>'})
						}),
						$('<th>', {
							colspan: 5,
							html: header
						}),
						$('<th>', {
							html: $('<a>', {click: function() {
								change_month(+1);
								update_calendar();
							}, html: '<svg width="12" height="14"><polygon points="0,0 0,14 12,7"></polygon></svg>'})
						})
					]
				}).on('wheel', function(e) {
					e.preventDefault();
					if (e.originalEvent.deltaY > 0)
						change_month(-1);
					else if (e.originalEvent.deltaY < 0)
						change_month(+1);
					update_calendar();
				}),
				$('<tr>', {
					html: [
						$('<th>', {text: 'Mon'}),
						$('<th>', {text: 'Tue'}),
						$('<th>', {text: 'Wed'}),
						$('<th>', {text: 'Thu'}),
						$('<th>', {text: 'Fri'}),
						$('<th>', {text: 'Sat'}),
						$('<th>', {text: 'Sun'})
					]
				})
			]
		})
	});

	var change_second = function(diff_num) {
		time.setSeconds(time.getSeconds() + diff_num);
	};
	var time_chooser = $('<table>', {
		class: 'time-chooser',
		html: $('<tbody>', {
			html: [
				$('<tr>', {
					html: [21600,600,10].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="50,27 30,19 10,27 30,6"></polygon></svg>'})
							.on('click', function() {
								change_second(+diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [3600,60,1].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="50,19 10,19 30,9"></polygon></svg>'})
							.on('click', function() {
								change_second(+diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [23,59,59].map(function(max_val, idx) {
						var input = $('<input>', {
							type: 'text',
							maxlength: 2,
							prop: {
								seconds_per_unit: [3600, 60, 1][idx],
								jump_seconds_per_unit: [21600,600,10][idx],
							}
						});
						var input_value_change = function() {
							var val = parseInt(input.val());
							if (isNaN(val) || val < 0 || val > max_val) {
								input.css('background-color', '#ff9393');
								return;
							}
							input.css('background-color', 'initial');

							if (idx === 0)
								time.setHours(val);
							else if (idx === 1)
								time.setMinutes(val);
							else if (idx === 2)
								time.setSeconds(val);

							update_calendar(true, false);
						};
						// Event handlers
						input.on('input', input_value_change);
						input.on('focusout', function() {
							var val = parseInt($(this).val());
							if (isNaN(val) || val < 0)
								val = '00';
							else if (val > max_val)
								val = max_val;
							else if (val < 10)
								val = '0' + val;
							input.val(val);
							input_value_change();
						});

						return $('<td>', {html: input});
					})
				}), $('<tr>', {
					html: [3600,60,1].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="10,11 50,11 30,21"></polygon></svg>'})
							.on('click', function() {
								change_second(-diff);
								update_calendar();
							})
						});
					})
				}), $('<tr>', {
					html: [21600,600,10].map(function(diff) {
						return $('<td>', {html:
							$('<a>', { html: '<svg width="60" height="30"><polygon points="10,3 30,11 50,3 30,24"></polygon></svg>'})
							.on('click', function() {
								change_second(-diff);
								update_calendar();
							})
						});
					})
				})
			]
		})
	});
	// Wheel handler on time_chooser
	time_chooser.find('tbody').on('wheel', function(e) {
		e.preventDefault();
		var tr = this.childNodes[2];
		var td = tr.childNodes[$(e.target).closest('td', this).index()];
		var input = td.childNodes[0];

		var sec_delta = (e.shiftKey ? input.jump_seconds_per_unit : input.seconds_per_unit);
		if (e.originalEvent.deltaY > 0)
			change_second(-sec_delta);
		else if (e.originalEvent.deltaY < 0)
			change_second(sec_delta);

		update_calendar();
	});

	// Days table
	var tbody, tbody_date = new Date(time);
	tbody_date.setDate(0); // Change month; this variable is used to skip
	                       // regenerating the whole table when it is unnecessary
	function update_calendar(update_datetime_input, update_time_inputs) {
		if (update_datetime_input !== false)
			$(datetime_info_input).val(date_to_datetime_str(time));

		// Time chooser
		if (update_time_inputs !== false) {
			var x = time_chooser.find('input');
			var foo = function(x) { return x < 10 ? '0' + x : x; };
			x.eq(0).val(foo(time.getHours()));
			x.eq(1).val(foo(time.getMinutes()));
			x.eq(2).val(foo(time.getSeconds()));
		}
		// Header
		header.text(months[time.getMonth()] + ' ' + time.getFullYear());
		// Tbody
		// Faster update - skipping regeneration of tbody
		if (time.getFullYear() === tbody_date.getFullYear() &&
			time.getMonth() === tbody_date.getMonth())
		{
			tbody.find('.chosen').removeClass('chosen');
			tbody.find('td:not(.gray)').eq(time.getDate() - 1).addClass('chosen');
			return;
		}

		tbody_date = new Date(time);
		tbody = $('<tbody>').on('wheel', function(e) {
			e.preventDefault();
			if (e.originalEvent.deltaY > 0)
				time.setDate(time.getDate() - 1);
			else if (e.originalEvent.deltaY < 0)
				time.setDate(time.getDate() + 1);

			update_calendar();
		});

		var day_onclick = function(that_elem_time) {
			var tet = new Date(that_elem_time.valueOf());
			return function() {
				// Update time
				time = new Date(tet.getFullYear(),
					tet.getMonth(),
					tet.getDate(),
					time.getHours(),
					time.getMinutes(),
					time.getSeconds());

				update_calendar();
			};
		};

		var tm = new Date(time.valueOf());
		tm.setDate(1);
		// Move tm back until it is Monday
		while (tm.getDay() != 1)
			tm.setDate(tm.getDate() - 1);
		// Fill all the table
		var passed_selected_day = false;
		var today = new Date();
		while (!passed_selected_day || tm.getMonth() === time.getMonth()) {
			// Add row (all the 7 days)
			var tr = $('<tr>');
			for (var i = 0; i < 7; ++i) {
				var td = $('<td>', {
					html: $('<a>', {
						text: tm.getDate(),
						click: day_onclick(tm)
					})
				});
				// Other month than currently selected
				if (tm.getMonth() !== time.getMonth())
					td.addClass('gray');
				// Mark today
				if (tm.getFullYear() === today.getFullYear() &&
					tm.getMonth() === today.getMonth() &&
					tm.getDate() === today.getDate())
				{
					td.addClass('today');
				}
				// Mark the selected day
				if (tm.getMonth() === time.getMonth() &&
					tm.getDate() === time.getDate())
				{
					passed_selected_day = true;
					td.addClass('chosen');
				}
				tr.append(td);

				tm.setDate(tm.getDate() + 1);
			}
			tbody.append(tr);
		}
		calendar.children('tbody').remove();
		calendar.append(tbody);
	}
	update_calendar();

	var round_up_5_minutes = function() {
		var k = time.getMinutes() + (time.getSeconds() !== 0) + 4;
		time.setMinutes(k - k % 5);
		time.setSeconds(0);
		update_calendar();
	};

	var save_changes = function() {
		$(text_input).val(date_to_datetime_str(time));
		$(hidden_input).val(Math.floor(time.getTime() / 1000));
	};

	var datetime_info_input = $('<input>', {
		type: 'text',
		class: 'calendar-input',
		value: date_to_datetime_str(time)
	});

	oldmodal($('<div>', {
		html: [
			$('<center>').append(datetime_info_input),
			calendar,
			time_chooser,
			$('<center>', {html:
				$('<a>', {
					class: 'btn-small',
					text: 'Round up 5 minutes',
					click: round_up_5_minutes
				})
			}),
			$('<center>', {html:
				$('<a>', {
					class: 'btn-small',
					text: 'Set to now',
					click: function() {
						time = new Date();
						update_calendar();
					}
				})
			}),
			$('<center>', {html:
				$('<a>', {
					class: 'btn blue',
					text: 'Save changes',
					click: function() {
						save_changes();
						close_oldmodal($(this).closest('.oldmodal'));
					}
				})
			}),
		]
	}), function(oldmodal_elem) {
		var keystorke_update = function(e) {
			// Editing combined datetime input field
			if (datetime_info_input.is(':focus')) {
				setTimeout(function() {
					// Dotetime validation
					var miliseconds_dt = Date.parse(datetime_info_input.val());
					if (isNaN(miliseconds_dt)) {
						datetime_info_input.css('background-color', '#ff9393');
					} else {
						// Propagate new datetime to the rest of calendar
						time = new Date(miliseconds_dt);
						datetime_info_input.css('background-color', 'initial');
						update_calendar(false);
						if (e.key == 'Enter') {
							save_changes();
							close_oldmodal(oldmodal_elem);
						}
					}
				});

				return;
			}

			// Time input field
			if (e.target.tagName === 'INPUT' && e.target.seconds_per_unit != null) {
				var sec_delta = (e.shiftKey ? e.target.jump_seconds_per_unit : e.target.seconds_per_unit);
				if (e.key === 'ArrowUp') {
					change_second(sec_delta);
					update_calendar();
				} else if (e.key === 'ArrowDown') {
					change_second(-sec_delta);
					update_calendar();
				}

				return;
			}

			if ($.contains(document.documentElement, month_chooser[0])) {
				if (e.key === 'ArrowUp')
					month_chooser.find('td > a').eq(0).click();
				else if (e.key === 'ArrowDown')
					month_chooser.find('td > a').eq(2).click();
				else
					return;

				e.preventDefault();
				return;
			}

			if (e.key === 'ArrowLeft')
				time.setDate(time.getDate() - 1);
			else if (e.key === 'ArrowRight')
				time.setDate(time.getDate() + 1);
			else if (e.key === 'ArrowUp')
				time.setDate(time.getDate() - 7);
			else if (e.key === 'ArrowDown')
				time.setDate(time.getDate() + 7);
			else
				return;

			e.preventDefault();
			update_calendar();
		};

		$(document).on('keydown', keystorke_update);
		oldmodal_elem[0].onoldmodalclose = function() {
			$(document).off('keydown', keystorke_update);
			if (date_to_datetime_str(time) != $(text_input).val()) {
				oldmodal($('<p>', {
					style: 'margin: 0; text-align: center',
					text: 'Your change to the datetime will be lost. Unsaved change: ' + date_to_datetime_str(time) + '.'
				}).add('<center>', {
					style: 'margin: 12px auto 0',
					html: $('<a>', {
						class: 'btn-small blue',
						text: 'Save changes',
						click: function() {
							save_changes();
							close_oldmodal($(this).closest('.oldmodal'));
							remove_oldmodals(oldmodal_elem);
						}
					}).add('<a>', {
						class: 'btn-small',
						text: 'Discard changes',
						click: function() {
							close_oldmodal($(this).closest('.oldmodal'));
							remove_oldmodals(oldmodal_elem);
						}
					})
				}), function(oldmodal) {
					oldmodal[0].onoldmodalclose = function() {
						remove_oldmodals(oldmodal_elem);
					};
				});

				return false;
			}
		};
	});

	datetime_info_input[0].select();
}
function dt_chooser_input(name, allow_neg_inf /* = false */, allow_inf /* = false */, copy_and_paste_buttons /* = false*/, initial_dt /* = undefined <=> use current time, Infinity <=> inf, -Infinity <=> -inf */, neg_inf_button_text /* = 'Set to -inf' */, inf_button_text /* = 'Set to inf' */) {
	/*const*/ var neg_inf_text_to_show = neg_inf_button_text;
	/*const*/ var inf_text_to_show = inf_button_text;

	if (neg_inf_button_text === undefined) {
		neg_inf_button_text = 'Set to -inf';
		neg_inf_text_to_show = '-inf';
	}
	if (inf_button_text === undefined) {
		inf_button_text = 'Set to inf';
		inf_text_to_show = 'inf';
	}

	var now_round_up_5_minutes = function() {
		var dt = new Date();
		// Round to 5 minutes
		var k = dt.getMinutes() + (dt.getSeconds() !== 0) + 4;
		dt.setMinutes(k - k % 5);
		dt.setSeconds(0);
		return dt;
	}

	if (initial_dt === undefined)
		initial_dt = now_round_up_5_minutes();

	var value_input = $('<input>', {
		type: 'hidden',
		name: name
	});

	var open_oldmodal_chooser = function() {
		var date_set = utcdt_or_tm_to_Date(value_input.val());
		if (date_set == Infinity || date_set == -Infinity)
			date_set = now_round_up_5_minutes();
		open_calendar_on(date_set, chooser_input, value_input);
	};

	var chooser_input = $('<input>', {
		type: 'text',
		class: 'calendar-input',
		readonly: true,
		click: open_oldmodal_chooser
	});

	var update_choosen_dt = function(new_dt) {
		if (new_dt === Infinity) {
			value_input.val('+inf');
			chooser_input.val(inf_text_to_show);
		} else if (new_dt === -Infinity) {
			value_input.val('-inf');
			chooser_input.val(neg_inf_text_to_show);
		} else {
			value_input.val(Math.floor(new_dt.getTime() / 1000));
			chooser_input.val(date_to_datetime_str(new_dt));
		}
	};

	update_choosen_dt(initial_dt);

	var buttons = $();
	if (allow_neg_inf) {
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: neg_inf_button_text,
			click: update_choosen_dt.bind(null, -Infinity)
		});
	}
	if (allow_inf) {
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: inf_button_text,
			click: update_choosen_dt.bind(null, Infinity)
		});
	}
	if (copy_and_paste_buttons) {
		buttons = buttons.add(copy_to_clipboard_btn(true, 'Copy', function() {
			localStorage.setItem('dt_chooser_clipboard', value_input.val());
			return chooser_input.val();
		}));
		buttons = buttons.add('<a>', {
			class: 'btn-small',
			text: 'Paste',
			style: 'position: relative',
			click: function() {
				let val = localStorage.getItem('dt_chooser_clipboard');
				if (val != null) {
					val = utcdt_or_tm_to_Date(val);
					if ((val === Infinity && !allow_inf) || (val == -Infinity && !allow_neg_inf)) {
						append_btn_tooltip($(this), 'Failed: invalid value in sim clipboard!');
					} else {
						update_choosen_dt(val);
						append_btn_tooltip($(this), 'Pasted!');
					}
				} else {
					append_btn_tooltip($(this), 'Failed: no datetime in sim clipboard!');
				}
			}
		});
	}

	return chooser_input.add(value_input).add(buttons);
}
