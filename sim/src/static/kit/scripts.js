function hex2str(hexx) {
	var hex = hexx.toString(); //force conversion
	var str = '';
	for (var i = 0; i < hex.length; i += 2)
		str += String.fromCharCode(parseInt(hex.substr(i, 2), 16));
	return str;
}
function text_to_safe_html(str) {
	return $('<div>', {text: str}).html();
}
function logged_user_id() {
	var x = $('.navbar .rightbar .user + ul > a:first-child').attr('href');
	return x.substr(x.lastIndexOf('/') + 1);
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
// Converts datetimes to local
function normalize_datetime(elem, add_tz) {
	elem = $(elem);
	var time;
	if (isNaN(elem.attr('datetime'))) {
		var args = elem.attr('datetime').split(/[- :]/);
		--args[1]; // fit month in [0, 11]
		time = new Date(Date.UTC.apply(this, args));
	} else
		time = new Date(elem.attr('datetime') * 1000);

	var month = time.getMonth() + 1;
	var day = time.getDate();
	var hours = time.getHours();
	var minutes = time.getMinutes();
	var seconds = time.getSeconds();
	month = (month < 10 ? '0' : '') + month;
	day = (day < 10 ? '0' : '') + day;
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;

	// Add the timezone part
	elem.html(String().concat(time.getFullYear(), '-', month, '-', day,
		' ', hours, ':', minutes, ':', seconds));
	if (add_tz)
		elem.append(tz_marker());

	return elem;
};

$(document).ready(function() {
	// Converts datetimes
	$('*[datetime]').each(function() {
		normalize_datetime($(this),
			$(this).parents('.submissions, .problems, .jobs, .files').length == 0);
	});
});
// Clock
$(document).ready(function updateClock() {
	if (updateClock.time_difference === undefined)
		updateClock.time_difference = window.performance.timing.responseStart - start_time;

	var time = new Date();
	time.setTime(time.getTime() - updateClock.time_difference);
	var hours = time.getHours();
	var minutes = time.getMinutes();
	var seconds = time.getSeconds();
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;
	// Update the displayed time
	document.getElementById('clock').innerHTML = String().concat(hours, ':', minutes, ':', seconds, tz_marker());
	setTimeout(updateClock, 1000 - time.getMilliseconds());
});
// Handle navbar correct size
function normalizeNavbar() {
	var navbar = $('.navbar');
	navbar.css('width', 'auto');

	if (navbar.outerWidth() <= $(window).width()) {
		navbar.css('width', '100%');
		navbar.css('position', 'fixed');
	} else {
		navbar.css('position', 'absolute');
		navbar.outerWidth($(document).width());
	}
}
$(document).ready(normalizeNavbar);
$(window).resize(normalizeNavbar);

// Handle menu correct size
function normalizeMenu() {
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
$(document).ready(normalizeMenu);
$(window).resize(normalizeMenu);

// Returns value of cookie @p name or ... TODO!!!
function getCookie(name) {
	name = name + '=';
	var arr = document.cookie.split(';');
	for (i = 0; i < arr.length; ++i) {
		var j = 0;
		while (arr[i].charAt(j) == ' ')
			++j;
		if (arr[i].indexOf(name, j) == j)
			return arr[i].substring(j + name.length, arr[i].length);
	}

	return '';
};

// Adding csrf token to a form
function addCsrfTokenTo(form) {
	var x = $(form);
	x.find('input[name="csrf_token"]').remove(); // Avoid duplication
	x.append('<input type="hidden" name="csrf_token" value="' +
		getCookie('csrf_token') + '">');
	return form;
}

// Adding csrf token just before submitting a form
$(document).ready(function() {
	$('form').submit(function() { addCsrfTokenTo(this); });
});

/* ================================= Loader ================================= */
function remove_loader(elem) {
	$(elem).children('.loader, .loader-info').remove();
}
function append_loader(elem) {
	remove_loader(elem);
	elem = $(elem);
	if (elem[0].style.animationName === undefined &&
		elem[0].style.WebkitAnimationName === undefined)
	{
		$(elem).append('<img class="loader" src="/kit/img/loader.gif">');
	} else
		$(elem).append($('<span>', {
			class: 'loader',
			html: $('<div>', {class: 'spinner'})
		}));
}
function show_success_via_loader(elem, html) {
	elem = $(elem);
	remove_loader(elem);
	elem.append($('<span>', {
		class: 'loader-info success',
		html: html
	}));
}
function show_error_via_loader(elem, response, err_status, try_again_handler) {
	if (err_status == 'success' || err_status == 'error' || err_status === undefined)
		err_status = '';
	else
		err_status = '; ' + err_status;

	elem = $(elem);
	remove_loader(elem);
	elem.append($('<span>', {
		class: 'loader-info error',
		html: $('<span>', {
			text: "Error: " + response.status + ' ' + response.statusText + err_status
		}).add(try_again_handler === undefined ? '' : $('<a>', {
			text: 'Try again',
			click: try_again_handler
		}))
	}));

	// Additional message
	var x = elem.find('.loader-info > span');
	try {
		var msg = $($.parseHTML(response.responseText)).text();

		if (msg != '')
			x.text(x.text().concat("\nInfo: ", msg));

	} catch (err) {
		if (response.responseText != '' // There is a message
			&& response.responseText.lastIndexOf('<!DOCTYPE html>', 0)
				!== 0 // Message is not a whole HTML page
			&& response.responseText.lastIndexOf('<!doctype html>', 0)
				!== 0) // Message is not a whole HTML page
		{
			x.text(x.text().concat("\nInfo: ",
				response.responseText));
		}
	}
}

/* ================================= Form ================================= */
var Form = {};
(function() {
	this.field_group = function(label, input_context) {
		return $('<div>', {
			class: 'field-group',
			html: $('<label>', {text: label}).add('<input>', input_context)
		});
	}

	this.send_via_ajax = function(form, url, success_msg /*= 'Success'*/, loader_parent)
	{
		if (success_msg === undefined)
			success_msg = 'Success';
		if (loader_parent === undefined)
			loader_parent = $(form);

		form = $(form);
		addCsrfTokenTo(form);
		append_loader(loader_parent);

		$.ajax({
			type: 'POST',
			url: url,
			data: form.serialize(),
			success: function() {
				show_success_via_loader(loader_parent, success_msg);
			},
			error: function(resp, status) {
				show_error_via_loader(loader_parent, resp, status);
			}
		});
		return false;
	}
}).call(Form);

function ajax_form(title, target, html, success_msg) {
	return $('<div>', {
		class: 'form-container',
		html: $('<h1>', {text: title})
		.add('<form>', {
			method: 'post',
			html: html
		}).submit(function () {
			return Form.send_via_ajax(this, target, success_msg);
		})
	});
}

/* ================================= Modals ================================= */
function close_modal(modal) {
	modal = $(modal);
	modal.remove();
	if (modal.is('.preview'))
		window.history.replaceState({}, '', modal.attr('prev-url'));
}
$(document).click(function(event) {
	if ($(event.target).is('.modal'))
		close_modal($(event.target));
	else if ($(event.target).is('.modal .close'))
		close_modal($(event.target).parent().parent());
});
function modal(modal_body) {
	return $('<div>', {
		class: 'modal',
		html: $('<div>', {
			html: $('<span>', { class: 'close' }).add(modal_body)
		})
	}).appendTo('body');
}
function centerize_modal(modal) {
	// Update padding-top
	var m = $(modal);
	var new_padding = (m.innerHeight() - m.children('div').innerHeight()) / 2;
	m.css({'padding-top': Math.max(new_padding, 0)});
}
function modal_request(title, form, target_url, success_msg) {
	var elem = modal($('<h2>', {text: title}));
	Form.send_via_ajax(form, target_url, success_msg, elem.children());
}
function dialogue_modal_request(title, info_html, go_text, go_classes, target_url, success_msg, cancel_text) {
	modal($('<h2>', {text: title})
		.add(info_html).add('<div>', {
			style: 'margin: 8px auto 0',
			html: $('<a>', {
				class: go_classes,
				text: go_text,
				click: function() {
					var form = $(this).parent().parent().find('form');
					if (form.length === 0)
						form = $('<form>');
					Form.send_via_ajax(form, target_url, success_msg,
						$(this).parent().parent());
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
/*function modalForm(form_title, form_body) {
	modal($('<h2>', {
		text: form_title
	}).add('<form>', {
		// enctype: 'application/x-www-form-urlencoded',
		style: 'display: flex; flex-flow: column nowrap',
		html: form_body
	}));
}
function sendModalFrom(url, success_msg) {
	var form = $('.modal form');
	addCsrfTokenTo(form);
	$('#modal_req_status').remove();
	$('<p>', {
		id: 'modal_req_status',
		text: 'Sending request...'
	}).appendTo(form);
	console.log(form.serialize());
	$.ajax({
		type: 'POST',
		url: url,
		data: form.serialize(),
		success: function() {
			$('#modal_req_status').html(success_msg + ' <a onclick="location.reload(true)">Reload the page</a> to see changes.')
				.css('background', '#9ff55f');
		},
		error: function(resp) {
			$('#modal_req_status').replaceWith($('<pre>', {
				id: 'modal_req_status',
				text: "Error: " + resp.status + ' ' + resp.statusText,
				style: 'background: #f55f5f'
			}));
			// Additional message
			try {
				var xml = $.parseXML(resp.responseText);
				var msg = $(xml).text();

				if (msg != '') {
					var x = $('#modal_req_status');
					x.text(x.text().concat("\nInfo: ", msg));
				}
			} catch (err) {
				if (resp.responseText != '' // There is a message
					&& resp.responseText.lastIndexOf('<!DOCTYPE html>', 0)
						!== 0 // Message is not a whole HTML page
					&& resp.responseText.lastIndexOf('<!doctype html>', 0)
						!== 0) // Message is not a whole HTML page
				{
					var x = $('#modal_req_status');
					x.text(x.text().concat("\nInfo: ",
						resp.responseText));
				}
			}

		}
	});
	return false;
}
function modalFormSubmitButton(value, url, success_msg, css_classes, cancel_button_message)
{
	// Default arguments
	if (css_classes === undefined)
		css_classes = 'blue';
	if (cancel_button_message === undefined)
		cancel_button_message = null;

	return $('<div>', {
		style: 'margin: 8px auto 0',
		html: $('<input>', {
			type: 'submit',
			class: 'btn-small ' + css_classes,
			value: value,
			click: function() {
				return sendModalFrom(url, success_msg);
			}
		}).add((cancel_button_message === null ? '' : $('<input>', {
			type: 'button',
			class: 'btn-small',
			value: cancel_button_message,
			click: function() {
				$(this).parent().parent().parent().parent().remove();
			}
		})))
	});
}*/

/* ================================ Preview ================================ */
function preview_base(as_modal, new_window_location, func) {
	if (as_modal) {
		var elem = $('<div>', {
			class: 'modal preview',
			'prev-url': window.location.href,
			html: $('<div>', {
				html: $('<span>', { class: 'close'})
				.add('<div>', {style: 'display:block'})
			})
		}).appendTo('body');
		window.history.replaceState({}, '', new_window_location);
		func.call(elem.find('div > div'));
		centerize_modal(elem);

	} else
		func.call($(document.body));
}
function preview_ajax(as_modal, ajax_url, success_handler, new_window_location) {
	preview_base(as_modal, new_window_location, function() {
		var elem = this;
		append_loader(elem);
		$.ajax({
			type: 'GET',
			url: ajax_url,
			dataType: 'json',
			success: function(data) {
				remove_loader(elem);

				if (data.length === 0)
					return show_error_via_loader(elem, {
						status: '404',
						statusText: 'Not Found'
					});

				success_handler.apply(elem, arguments);

				if (as_modal)
					centerize_modal(elem.parent().parent());
			},
			error: function(resp, status) {
				show_error_via_loader(elem, resp, status, function () {
					impl(elem);
				});
			}
			});
	});
}
function a_preview_button(href, text, classes, func) {
	return $('<a>', {
			'href': href,
			'text': text,
			class: classes,
			click: function(event) {
				if (event.ctrlKey)
					return true; // Allow opening the link in a new tab
				func();
				return false;
			}

		});
}

/* ================================= Lister ================================= */
function Lister(elem) {
	this.elem = $(elem);
	this.lock = false;

	this.fetch_more = function() {
		if (this.lock)
			return;

		this.lock = true;
		append_loader(this.elem.parent());

		this.fetch_more_impl();
	}

	this.monitor_scroll = function() {
		var obj = this;

		var modal_parent = elem.closest('.modal');
		if (modal_parent.length === 1)
			modal_parent.on('scroll resize', function() {
				var height = $(this).children('div').height();
				var scroll_top = $(this).scrollTop();
				if (height - $(window).height() - scroll_top <= 300)
					obj.fetch_more();
			});
		else
			$(document).on('scroll resize', function() {
				var x = $(this);
				if (x.height() - $(window).height() - x.scrollTop() <= 300)
					obj.fetch_more();
			});

		modal_parent = null;
	};

	this.get_error_handler = function() {
		var obj = this;
		return function(resp, status) {
			show_error_via_loader(obj.elem.parent(), resp, status,
				function () {
					obj.lock = false;
					obj.fetch_more();
				});
		};
	}
}

////////////////////////////////////////////////////////////////////////////////

/* ================================== Logs ================================== */
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
	};

	function contains(idx, str) {
		return (log.substring(idx, idx + str.length) == str);
	};
	for (var i = 0; i < end; ++i) {
		if (contains(i, '\033[31m')) {
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
function Logs(type, elem) {
	this.type = type;
	this.elem = $(elem);
	this.offset = undefined;
	this.lock = false; // allow only manual unlocking

	this.fetch_more = function() {
		if (this.lock)
			return;
		this.lock = true;

		append_loader(this.elem);
		var logs = this;
		$.ajax({
			type: 'GET',
			url: '/api/logs/' + logs.type +
				(logs.offset === undefined ? '' : '?' + logs.offset),
			success: function(data) {
				data = String(data).split('\n');
				logs.offset = data[0];
				data = hex2str(data[1]);

				var prev_height = elem[0].scrollHeight;
				var prev = prev_height - elem.scrollTop();

				remove_loader(logs.elem);
				elem.html(colorize(text_to_safe_html(data) + elem.html(),
					data.length + 2000));
				var curr_height = elem[0].scrollHeight;
				elem.scrollTop(curr_height - prev);

				// Load more logs if scrolling up did not become possible
				if (logs.offset > 0) {
					logs.lock = false;
					if (elem.innerHeight() >= curr_height || prev_height == curr_height) {
						// avoid recursion
						setTimeout(function(){ logs.fetch_more(); }, 0);
					}
				}
			},
			error: function(resp, status) {
				show_error_via_loader(logs.elem, resp, status,
					function () {
						logs.lock = false; // allow only manual unlocking
						logs.fetch_more();
					});
			}
		});
	};

	this.monitor_scroll = function() {
		var logs = this;
		this.elem.on('scroll', function() {
			if ($(this).scrollTop() <= 300)
				logs.fetch_more();
		});
	};

	this.fetch_more();
}

/* ============================ Actions buttons ============================ */
var ActionsToHTML = {};
(function() {
	this.job = function(job_id, actions_str, problem_id, show_view_job /*= true*/) {
		if (show_view_job == undefined)
			show_view_job = true;

		var res = [];
		if (show_view_job)
			res.push(a_preview_button('/jobs/' + job_id, 'View', 'btn-small',
				function() { preview_job(true, job_id); }));

		if (actions_str.indexOf('P') !== -1 || actions_str.indexOf('R') !== -1)
			res.push($('<div>', {
				class: 'dropmenu down',
				html: $('<a>', {
					class: 'btn-small dropmenu-toggle',
					text: 'Download'
				}).add('<ul>', {
					html: $('<a>', {
						href: '/api/job/' + job_id + '/report',
						text: 'Report'
					}).add(actions_str.indexOf('P') === -1 ? '' : $('<a>', {
						href: '/api/job/' + job_id + '/uploaded-package',
						text: 'Uploaded package'
					}))
				})
			}));

		if (actions_str.indexOf('V') !== -1)
			res.push(a_preview_button('/p/' + problem_id, 'View problem',
				'btn-small green', function() { preview_problem(true, problem_id); }));

		if (actions_str.indexOf('c') !== -1)
			res.push($('<a>', {
				class: 'btn-small red',
				text: 'Cancel job',
				click: function() {
					var jid = job_id;
					return function() { cancel_job(jid); }
				}()
			}));

		if (actions_str.indexOf('r') !== -1)
			res.push($('<a>', {
				class: 'btn-small orange',
				text: 'Restart job',
				click: function() {
					var jid = job_id;
					return function() { restart_job(jid); }
				}()
			}));

		return res;
	}

	this.user = function(user_id, actions_str, show_view_user /*= true*/) {
		if (show_view_user === undefined)
			show_view_user = true;

		var res = [];
		if (show_view_user && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/u/' + user_id, 'View', 'btn-small',
				function() { preview_user(true, user_id); }));

		if (actions_str.indexOf('E') !== -1)
			res.push(a_preview_button('/u/' + user_id + '/edit', 'Edit',
				'btn-small blue', function() { edit_user(true, user_id); }));

		if (actions_str.indexOf('D') !== -1)
			res.push(a_preview_button('/u/' + user_id + '/delete', 'Delete',
				'btn-small red', function() { delete_user(true, user_id); }));

		if (actions_str.indexOf('P') !== -1 || actions_str.indexOf('p') !== -1)
			res.push(a_preview_button('/u/' + user_id + '/change-password',
				'Change password', 'btn-small orange',
				function() { change_user_password(true, user_id); }));

		return res;
	}

	this.submission = function(submission_id, actions_str, show_view_submission /*= true*/) {
		if (show_view_submission === undefined)
			show_view_submission = true;

		var res = [];
		if (show_view_submission && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/s/' + submission_id, 'Preview', 'btn-small',
				function() { preview_submission(true, submission_id); }));

		if (actions_str.indexOf('s') !== -1) {
			res.push(a_preview_button('/s/' + submission_id + '/source', 'Source',
				'btn-small', function() { submission_source(true, submission_id); }));

			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/submission/' + submission_id + '/download',
				text: 'Download'
			}));
		}

		if (actions_str.indexOf('C') !== -1)
			res.push(a_preview_button('/s/' + submission_id + '/chtype', 'Change type',
				'btn-small orange',
				function() { submission_chtype(true, submission_id); }));

		if (actions_str.indexOf('R') !== -1)
			res.push($('<a>', {
				class: 'btn-small blue',
				text: 'Rejudge',
				click: function() {
					var sid = submission_id;
					return function() { rejudge_submission(sid); }
				}()
			}));

		if (actions_str.indexOf('D') !== -1)
			res.push($('<a>', {
				class: 'btn-small red',
				text: 'Delete',
				click: function() {
					var sid = submission_id;
					return function() { delete_submission(sid); }
				}()
			}));

		return res;
	}
}).call(ActionsToHTML);

/* ================================= Users ================================= */
function add_user(as_modal) {
	preview_base(as_modal, '/u/add', function() {
		this.append(ajax_form('Add user', '/api/user/add',
			Form.field_group('Username', {
					type: 'text',
					name: 'username',
					size: 24,
					// maxlength: 'TODO...',
					required: true
				}).add($('<div>', {
					class: 'field-group',
					html: $('<label>', {text: 'Type'})
					.add('<select>', {
						name: 'type',
						required: true,
						html: $('<option>', {
							value: 'A',
							text: 'Admin',
						}).add('<option>', {
							value: 'T',
							text: 'Teacher',
						}).add('<option>', {
							value: 'N',
							text: 'Normal',
							selected: true
						})
					})
				})).add(Form.field_group('First name', {
					type: 'text',
					name: 'first_name',
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add(Form.field_group('Last name', {
					type: 'text',
					name: 'last_name',
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add(Form.field_group('Email', {
					type: 'email',
					name: 'email',
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add(Form.field_group('Password', {
					type: 'password',
					name: 'pass',
					size: 24,
				})).add(Form.field_group('Password (repeat)', {
					type: 'password',
					name: 'pass1',
					size: 24,
				})).add('<div>', {
					html: $('<input>', {
						class: 'btn blue',
						type: 'submit',
						value: 'Submit'
					})
				}), 'User was added'
			));
	})
}
function preview_user(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		data = data[0];

		this.append($('<div>', {
			class: 'header',
			html: $('<span>', {
				style: 'margin: auto 0',
				html: $('<a>', {
					href: '/u/' + user_id,
					text: data[1]
				})
			}).append(text_to_safe_html(' (' + data[2] + ' ' + data[3] + ')'))
			.add('<div>', {
				html: ActionsToHTML.user(user_id, data[6], false)
			})
		})).append($('<center>', {
			html: $('<div>', {
				class: 'user-info',
				html: $('<div>', {
					class: 'first-name',
					html: $('<label>', {text: 'First name'})
				}).append(text_to_safe_html(data[2]))
				.add($('<div>', {
					class: 'last-name',
					html: $('<label>', {text: 'Last name'})
				}).append(text_to_safe_html(data[3])))
				.add($('<div>', {
					class: 'username',
					html: $('<label>', {text: 'Username'})
				}).append(text_to_safe_html(data[1])))
				.add($('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Account type'}).add('<span>', {
						class: data[5],
						text: String(data[5]).slice(0, 1).toUpperCase() +
							String(data[5]).slice(1)
					})
				}))
				.add($('<div>', {
					class: 'email',
					html: $('<label>', {text: 'Email'})
				}).append(text_to_safe_html(data[4])))
			})
		}));

		var s_table = $('<table>', {
			class: 'submissions'
		});
		this.append("<h2>User's submissions</h2>").append(s_table);
		new SubmissionsLister(s_table, '/u' + user_id).monitor_scroll();

	}, '/u/' + user_id);
}
function edit_user(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		data = data[0];

		var actions = data[6];
		if (actions.indexOf('E') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit account', '/api/user/' + user_id + '/edit',
			Form.field_group('Username', {
					type: 'text',
					name: 'username',
					value: data[1],
					size: 24,
					// maxlength: 'TODO...',
					required: true
				}).add($('<div>', {
					class: 'field-group',
					html: $('<label>', {text: 'Type'})
					.add('<select>', {
						name: 'type',
						required: true,
						html: function() {
							var res = [];
							if (actions.indexOf('A') !== -1)
								res.push($('<option>', {
									value: 'A',
									text: 'Admin',
									selected: ('admin' === data[5] ? true : undefined)
								}));

							if (actions.indexOf('T') !== -1)
								res.push($('<option>', {
									value: 'T',
									text: 'Teacher',
									selected: ('teacher' === data[5] ? true : undefined)
								}));

							if (actions.indexOf('N') !== -1)
								res.push($('<option>', {
									value: 'N',
									text: 'Normal',
									selected: ('normal' === data[5] ? true : undefined)
								}));

							return res;
						}()
					})
				})).add(Form.field_group('First name', {
					type: 'text',
					name: 'first_name',
					value: data[2],
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add(Form.field_group('Last name', {
					type: 'text',
					name: 'last_name',
					value: data[3],
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add(Form.field_group('Email', {
					type: 'email',
					name: 'email',
					value: data[4],
					size: 24,
					// maxlength: 'TODO...',
					required: true
				})).add('<div>', {
					html: $('<input>', {
						class: 'btn blue',
						type: 'submit',
						value: 'Update'
					})
				})
			));

	}, '/u/' + user_id + "/edit");
}
function delete_user(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		data = data[0];

		var actions = data[6];
		if (actions.indexOf('D') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		var title = 'Delete user ' + user_id;
		var confirm_text = 'Delete user';
		var p = $('<p>', {
			style: 'margin: 0 0 20px; text-align: center',
			html: 'You are going to delete the user '
		}).append(a_preview_button('/u/' + user_id, data[1], undefined, function() {
			preview_user(true, user_id);
		})).append('. As it cannot be undone,<br>you have to confirm it with YOUR password.');

		if (user_id == logged_user_id()) {
			title = confirm_text = 'Delete account';
			p.html('You are going to delete your account. As it cannot be undone,<br>you have to confirm it with your password.');
		}

		this.append(ajax_form(title,
			'/api/user/' + user_id + '/delete',
			p.add(Form.field_group('Your password', {
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
					href: (as_modal ? undefined : '/'),
					text: 'Go back',
					click: (as_modal ? function() {
						close_modal($(this).closest('.modal'));
					} : undefined)
				})
			}), 'Deletion succeeded'));

	}, '/u/' + user_id + "/delete");
}
function change_user_password(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		data = data[0];

		var actions = data[6];
		if (actions.indexOf('P') === -1 && actions.indexOf('p') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		if (actions.indexOf('P') === -1 && $('.navbar .user > strong').text() != data[1])
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Change password',
			'/api/user/' + user_id + '/change-password',
			(actions.indexOf('P') !== -1 ? $() : Form.field_group('Old password', {
				type: 'password',
				name: 'old_pass',
				size: 24,
			})).add(Form.field_group('New password', {
				type: 'password',
				name: 'new_pass',
				size: 24,
			})).add(Form.field_group('New password (repeat)', {
				type: 'password',
				name: 'new_pass1',
				size: 24,
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			}), 'Password changed'));

	}, '/u/' + user_id + "/change-password");
}
function UsersLister(elem) {
	Lister.call(this, elem);
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: '/api/users' + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (elem.children('thead').length === 0)
					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							'<th class="username">Username</th>' +
							'<th class="first-name">First name</th>' +
							'<th class="last-name">Last name</th>' +
							'<th class="email">Email</th>' +
							'<th class="type">Type</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');

				for (x in data) {
					x = data[x];
					obj.query_suffix = '/>' + x[0];

					var row = $('<tr>');
					row.append($('<td>', {text: x[0]}));
					row.append($('<td>', {text: x[1]}));
					row.append($('<td>', {text: x[2]}));
					row.append($('<td>', {text: x[3]}));
					row.append($('<td>', {text: x[4]}));
					row.append($('<td>', {
						class: x[5],
						text: String(x[5]).slice(0, 1).toUpperCase() +
							String(x[5]).slice(1)
					}));

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.user(x[0], x[6])
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(elem.parent());

				if (data.length == 0)
					return; // No more data to load

				obj.lock = false;
				if (obj.elem.height() - $(window).height() <= 300) {
					// Load more if scrolling down did not become possible
					setTimeout(function(){ obj.fetch_more(); }, 0); // avoid recursion
				}
			},
			error: obj.get_error_handler()
		});
	}

	this.fetch_more();
}

/* ================================== Jobs ================================== */
function preview_job(as_modal, job_id) {
	preview_ajax(as_modal, '/api/jobs/=' + job_id, function(data) {
		data = data[0];

		function info_html(info) {
			var td = $('<td>', {
				style: 'text-align:left'
			});
			for (var name in info) {
				td.append($('<label>', {text: name}));

				if (name == "submission")
					td.append(a_preview_button('/s/' + info[name], info[name],
						undefined, function() { preview_submission(true, info[name]); }));
				else if (name == "problem")
					td.append(a_preview_button('/p/' + info[name], info[name],
						undefined, function() { preview_problem(true, info[name]); }));
				else
					td.append(info[name]);

				td.append('<br>');
			}

			return td;
		};

		this.append($('<div>', {
			class: 'job-info',
			html: $('<div>', {
				class: 'header',
				html: $('<h1>', {
					text: 'Job ' + job_id
				}).add('<div>', {
					html: ActionsToHTML.job(job_id, data[8], data[7].problem,
						false)
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
							text: data[2]
						}).add(normalize_datetime($('<td>', {
								datetime: data[1],
								text: data[1]
							}), true)
						).add('<td>', {
							class: 'status ' + data[3][0],
							text: data[3][1]
						}).add('<td>', {
							html: data[5] === null ? 'System' :
								(data[6] == null ? 'Deleted (id: ' + data[5] + ')'
								: a_preview_button(
								'/u/' + data[5], data[6], undefined,
								function() { preview_user(true, data[5]); }))
						}).add('<td>', {
							html: info_html(data[7])
						})
					})
				})
			})
		})).append('<h2>Report preview</h2>')
		.append($('<pre>', {
			class: 'report-preview',
			html: colorize(text_to_safe_html(data[9][1]))
		}));

		if (data[9][0])
			this.append($('<p>', {
				text: 'The report is too large to show it entirely here. If you want to see the whole, click: '
			}).append($('<a>', {
				class: 'btn-small',
				href: '/api/job/' + job_id + '/report',
				text: 'Download the full report'
			})));
	}, '/jobs/' + job_id);
}
function cancel_job(job_id) {
	modal_request('Canceling job ' + job_id, $('<form>'),
		'/api/job/' + job_id + '/cancel', 'The job has been canceled.');
}
function restart_job(job_id) {
	dialogue_modal_request('Restart job', $('<label>', {
			text: 'Are you sure to restart the '
		}).append(a_preview_button('/jobs/' + job_id, 'job ' + job_id, undefined,
				function() { preview_job(true, job_id);}))
		.append('?'),
		'Restart job', 'btn-small orange', '/api/job/' + job_id + '/restart',
		'The job has been restarted.', 'No, go back');
}
function JobsLister(elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	Lister.call(this, elem);
	this.query_url = '/api/jobs' + query_suffix;
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: obj.query_url + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (elem.children('thead').length === 0) {
					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							'<th class="type">Type</th>' +
							'<th class="priority">Priority</th>' +
							'<th class="added">Added</th>' +
							'<th class="status">Status</th>' +
							'<th class="owner">Owner</th>' +
							'<th class="info">Info</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
					add_tz_marker(elem.find('thead th.added'));
				}

				for (x in data) {
					x = data[x];
					obj.query_suffix = '/<' + x[0];

					var row = $('<tr>');
					row.append($('<td>', {text: x[0]}));
					row.append($('<td>', {text: x[2]}));
					row.append($('<td>', {text: x[4]}));
					row.append($('<td>', {
						html: normalize_datetime(
							a_preview_button('/jobs/' + x[0], x[1], undefined,
								function() {
									var job_id = x[0];
									return function() {
										preview_job(true, job_id);
									};
								}()).attr('datetime', x[1]),
							false)
					}));
					row.append($('<td>', {
						class: 'status ' + x[3][0],
						text: x[3][1]
					}));
					row.append($('<td>', {
						html: x[5] === null ? 'System' : (x[6] == null ? x[5]
							: a_preview_button('/u/' + x[5], x[6], undefined, function() {
								var uid = x[5];
								return function() { preview_user(true, uid); };
							}()))
					}));
					// Info
					var info = x[7];
					row.append(function() {
						var td = $('<td>');
						function append_tag(name, val) {
							td.append($('<label>', {text: name}));
							td.append(val);
						}

						if (info.submission !== undefined)
							append_tag('submission',
								a_preview_button('/s/' + info.submission,
									info.submission, undefined, function() {
										preview_submission(true, info.submission);
									}));

						if (info.problem !== undefined)
							append_tag('problem',
								a_preview_button('/s/' + info.problem,
									info.problem, undefined, function() {
										preview_submission(true, info.problem);
									}));

						var names = ['name', 'memory limit', 'make public'];
						for (var idx in names) {
							var x = names[idx];
							if (info[x] !== undefined)
								append_tag(x, info[x]);
						}

						return td;
					}());

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.job(x[0], x[8], info.problem)
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(obj.elem.parent());

				if (data.length == 0)
					return; // No more data to load

				obj.lock = false;
				if (obj.elem.height() - $(window).height() <= 300) {
					// Load more if scrolling down did not become possible
					setTimeout(function(){ obj.fetch_more(); }, 0); // avoid recursion
				}
			},
			error: obj.get_error_handler()
		});
	};

	this.fetch_more();
}

/* ============================== Submissions ============================== */
/*function changeSubmissionType(submission_id, stype) {
	modalForm('Change submission type',
		$('<div>', {
			html: $('<label>', {
				text: 'New submission type: '
			}).add('<select>', {
				name: 'stype',
				html: $('<option>', {
					value: 'n/f',
					text: 'Normal / final',
					selected: (stype === 'n/f')
				}).add('<option>', {
					value: 'i',
					text: 'Ignored',
					selected: (stype === 'i')
				})
			})
		}).add(modalFormSubmitButton('Change type',
			'/s/' + submission_id + '/change-type',
			'Submission type has been changed.'))
	);
}
function rejudgeSubmission(submission_id) {
	modalForm('Rejudge submission ' + submission_id);
	sendModalFrom('/s/' + submission_id + '/rejudge',
		'Rejudge of the submission was added to the job queue.')
}
function rejudgeProblemSubmissions(problem_id) {
	modalForm('Rejudge submissions to the problem ' + problem_id,
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to rejudge all the submissions to the problem <a href="/p/' + problem_id +
					'">' + problem_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Rejudge submissions',
			'/p/' + problem_id + '/rejudge',
			'Rejudge of the submissions was added to the job queue', 'blue',
			'No, go back'))
	);
}
function rejudgeRoundSubmissions(round_id) {
	modalForm('Rejudge submissions in the round',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to rejudge all the submissions in the round <a href="/c/' + round_id +
					'">' + round_id + '</a>?',
			})
		}).add(modalFormSubmitButton('Rejudge submissions',
			'/c/' + round_id + '/edit/rejudge',
			'Rejudge of the submissions was added to the job queue', 'blue',
			'No, go back'))
	);
}*/

function SubmissionsLister(elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	this.show_user = (query_suffix.indexOf('/u') === -1 &&
		query_suffix.indexOf('/tS') === -1);

	this.show_contest = (query_suffix.indexOf('/C') === -1 &&
		query_suffix.indexOf('/R') === -1 &&
		query_suffix.indexOf('/P') === -1);

	Lister.call(this, elem);
	this.query_url = '/api/submissions' + query_suffix;
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: obj.query_url + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (elem.children('thead').length === 0) {
					if (data.length == 0) {
						obj.elem.parent().append($('<center>', {
							html: '<p>There are no submissions to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							(obj.show_user ? '<th class="username">Username</th>' : '') +
							'<th class="type">Type</th>' +
							'<th class="time">Added</th>' +
							'<th class="problem">Problem</th>' +
							'<th class="status">Status</th>' +
							'<th class="score">Score</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
					add_tz_marker(elem.find('thead th.time'));

				}

				for (x in data) {
					x = data[x];
					obj.query_suffix = '/<' + x[0];

					var row = $('<tr>', {
						class: (x[1] === 'Ignored' ? 'ignored' : undefined)
					});
					// Id
					row.append($('<td>', {text: x[0]}));

					// Username
					if (obj.show_user)
						row.append($('<td>', {
							html: x[2] === null ? 'System' : (x[3] == null ? x[2]
								: a_preview_button('/u/' + x[2], x[3], undefined,
									function() {
										var uid = x[2];
										return function() { preview_user(true, uid); };
									}()))
						}));

					// Type
					row.append($('<td>', {text: x[1]}));

					// Submission time
					row.append($('<td>', {
						html: normalize_datetime(
							a_preview_button('/s/' + x[0], x[12], undefined,
								function() {
									var submission_id = x[0];
									return function() {
										preview_submission(true, submission_id);
									};
								}()).attr('datetime', x[12]),
							false)
					}));

					// Problem
					if (x[6] == null) // Not in the contest
						row.append($('<td>', {
							html: a_preview_button('/p/' + x[4], x[5], undefined,
								function() {
									var pid = x[4];
									return function() { preview_problem(true, pid); };
							}())
						}));
					else
						row.append($('<td>', {
							html: [(obj.show_contest ? $('<a>', {
									href: '/c/' + x[10],
									text: x[11]
								}) : ''),
								(obj.show_contest ? ' ~> ' : ''),
								$('<a>', {
									href: '/c/' + x[8],
									text: x[9]
								}),
								' ~> ',
								$('<a>', {
									href: '/c/' + x[6],
									text: x[7]
								})
							]
						}))

					// Status
					row.append($('<td>', {
						class: 'status ' + x[13][0],
						text: (x[13][0].lastIndexOf('initial') === -1 ? ''
							: 'Initial: ') + x[13][1]
					}));

					// Score
					row.append($('<td>', { text: x[14] }));

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.submission(x[0], x[15])
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(obj.elem.parent());
				centerize_modal(obj.elem.parents('.modal'));

				if (data.length == 0)
					return; // No more data to load

				obj.lock = false;
				if (obj.elem.height() - $(window).height() <= 300) {
					// Load more if scrolling down did not become possible
					setTimeout(function(){ obj.fetch_more(); }, 0); // avoid recursion
				}
			},
			error: obj.get_error_handler()
		});
	};

	this.fetch_more();
}

/* ============================ Contest's users ============================ */
/*function addContestUser(contest_id) {
	modalForm('Add user to the contest',
		$('<div>', {
			class: 'field-group',
			style: 'margin-top: 5px',
			html: $('<div>', {
				html: $('<select>', {
				name: 'user_value_type',
					html: $('<option>', {
						value: 'username',
						text: 'Username',
						selected: true
					}).add('<option>', {
						value: 'uid',
						text: 'Uid'
					})
				})
			}).add('<input>', {
				type: 'text',
				name: 'user'
			})
		}).add('<div>', {
			class: 'field-group',
			html: $('<label>', {
				text: 'User mode'
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: true
				}).add('<option>', {
					value: 'm',
					text: 'Moderator'
				})
			})
		}).add(modalFormSubmitButton('Add user',
			'/c/' + contest_id + '/users/add',
			'User has been added.'))
	);
}
function changeContestUserMode(contest_id, user_id, mode) {
	modalForm('Change user mode',
		$('<div>', {
			html: $('<label>', {
				text: "New user's mode: "
			}).add('<select>', {
				name: 'mode',
				html: $('<option>', {
					value: 'c',
					text: 'Contestant',
					selected: (mode === 'c')
				}).add('<option>', {
					value: 'm',
					text: 'Moderator',
					selected: (mode === 'm')
				})
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Change mode',
			'/c/' + contest_id + '/users/change-mode',
			'User mode has been changed.'))
	);
}
function expelContestUser(contest_id, user_id, username) {
	modalForm('Expel user from the contest',
		$('<div>', {
			html: $('<label>', {
				html: 'Are you sure to expel the user <a href="/u/' + user_id +
					'">' + username + '</a> with the id = ' + user_id + '?',
			}).add('<input>', {
				type: 'hidden',
				name: 'uid',
				value: user_id
			})
		}).add(modalFormSubmitButton('Of course!',
			'/c/' + contest_id + '/users/expel',
			'User has been expelled.', 'red',
			'No, the user may stay'))
	);
}*/
