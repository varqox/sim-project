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
function is_logged_in() {
	return ($('.navbar .rightbar .user + ul > a:first-child').length > 0);
}
function logged_user_id() {
	var x = $('.navbar .rightbar .user + ul > a:first-child').attr('href');
	return x.substr(x.lastIndexOf('/') + 1);
}
function logged_user_is_admin() {
	return ($('.navbar > div > a[href="/logs"]').length === 1);
}
function logged_user_is_tearcher_or_admin() {
	return ($('.navbar > div > a[href="/u"]').length === 1);
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
function date_to_datetime_str(date) {
	var month = date.getMonth() + 1;
	var day = date.getDate();
	var hours = date.getHours();
	var minutes = date.getMinutes();
	var seconds = date.getSeconds();
	month = (month < 10 ? '0' : '') + month;
	day = (day < 10 ? '0' : '') + day;
	hours = (hours < 10 ? '0' : '') + hours;
	minutes = (minutes < 10 ? '0' : '') + minutes;
	seconds = (seconds < 10 ? '0' : '') + seconds;

	return String().concat(date.getFullYear(), '-', month, '-', day,
		' ', hours, ':', minutes, ':', seconds);
}
// Converts datetimes to local
function normalize_datetime(elem, add_tz) {
	elem = $(elem);
	if (elem.attr('datetime') === undefined)
		return elem;

	var time;
	if (isNaN(elem.attr('datetime'))) {
		var args = elem.attr('datetime').split(/[- :]/);
		--args[1]; // fit month in [0, 11]
		time = new Date(Date.UTC.apply(this, args));
	} else
		time = new Date(elem.attr('datetime') * 1000);

	// Add the timezone part
	elem.html(date_to_datetime_str(time));
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
	for (var i = 0; i < arr.length; ++i) {
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

function StaticMap() {
	this.data = new Array();

	this.cmp = function(a, b) { return a[0] - b[0]; }

	this.add = function(key, val) {
		this.data.push([key, val]);
	}

	this.prepare = function() { this.data.sort(this.cmp); }

	this.get = function(key) {
		if (this.data.length === 0)
			return null;

		var l = 0, r = this.data.length;
		while (l < r) {
			var m = (l + r) >> 1;
			if (this.data[m][0] < key)
				l = m + 1;
			else
				r = m;
		}

		return (this.data[l][0] == key ? this.data[l][1] : null);
	}
}

/* ============================ URL hash parser ============================ */
var url_hash_parser = {};
(function () {
	var args = window.location.hash; // Must begin with '#'
	if (args == '')
		args = '#';
	var beg = 0; // Points to the '#' just before the next argument

	this.next_arg  = function() {
		var pos = args.indexOf('#', beg + 1);
		if (pos === -1)
			return args.substr(beg + 1);

		return args.substring(beg + 1, pos);
	}

	this.extract_next_arg  = function() {
		var pos = args.indexOf('#', beg + 1);
		if (pos === -1) {
			if (beg >= args.length)
				return '';

			var res = args.substr(beg + 1);
			beg = args.length;
			return res;
		}

		var res = args.substring(beg + 1, pos);
		beg = pos;
		return res;
	}

	this.empty = function() { return (beg >= args.length); }

	this.assign = function(new_hash) {
		beg = 0;
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
	}

	this.assign_as_parsed = function(new_hash) {
		if (new_hash.charAt(0) !== '#')
			args = '#' + new_hash;
		else
			args = new_hash;
		beg = args.length;
	}


	this.append = function(next_args) {
		if (next_args.charAt(0) !== '#')
			args += '#' + next_args;
		else
			args += next_args;
	}

	this.parsed_prefix = function() { return args.substr(0, beg); }
}).call(url_hash_parser);

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
			processData: false,
			contentType: false,
			data: new FormData(form[0]),
			success: function(resp) {
				if (typeof success_msg === "function") {
					success_msg.call(form, resp);
				} else
					show_success_via_loader(loader_parent, success_msg);
			},
			error: function(resp, status) {
				show_error_via_loader(loader_parent, resp, status);
			}
		});
		return false;
	}
}).call(Form);

function ajax_form(title, target, html, success_msg, classes) {
	return $('<div>', {
		class: 'form-container' + (classes === undefined ? '' : ' ' + classes),
		html: $('<h1>', {text: title})
		.add('<form>', {
			method: 'post',
			html: html
		}).submit(function() {
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
$(document).keydown(function(event) {
	if (event.key == 'Escape')
		close_modal($('body').children('.modal').last());
});
function modal(modal_body) {
	return $('<div>', {
		class: 'modal',
		html: $('<div>', {
			html: $('<span>', { class: 'close' }).add(modal_body)
		})
	}).appendTo('body');
}
function centerize_modal(modal, allow_lowering /*= true*/) {
	if (allow_lowering === undefined)
		allow_lowering = true;

	// Update padding-top
	var m = $(modal);
	var new_padding = (m.innerHeight() - m.children('div').innerHeight()) / 2;

	if (!allow_lowering) {
		var old_padding = m.css('padding-top');
		if (old_padding !== undefined && parseInt(old_padding.slice(0, -2)) < new_padding)
			return;
	}

	m.css({'padding-top': Math.max(new_padding, 0)});
}
function modal_request(title, form, target_url, success_msg) {
	var elem = modal($('<h2>', {text: title}));
	Form.send_via_ajax(form, target_url, success_msg, elem.children());
}
function dialogue_modal_request(title, info_html, go_text, go_classes, target_url, success_msg, cancel_text, remove_buttons_on_click) {
	modal($('<h2>', {text: title})
		.add(info_html).add('<div>', {
			style: 'margin: 12px auto 0',
			html: $('<a>', {
				class: go_classes,
				text: go_text,
				click: function() {
					var loader_parent = $(this).parent().parent();
					var form = loader_parent.find('form');
					if (form.length === 0)
						form = $('<form>');

					if (remove_buttons_on_click)
						$(this).parent().remove();

					Form.send_via_ajax(form, target_url, success_msg,
						loader_parent);
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

function API_call(ajax_url, success_handler, loader_parent) {
	var thiss = this;
	var args = arguments;
	append_loader(loader_parent);
	$.ajax({
		type: 'GET',
		url: ajax_url,
		dataType: 'json',
		success: function(data) {
			remove_loader(loader_parent);
			success_handler.apply(this, arguments);
		},
		error: function(resp, status) {
			show_error_via_loader(loader_parent, resp, status, function () {
				// Avoid recursion
				setTimeout(function(){ API_call.apply(thiss, args); }, 0);
			});
		}
	});
}

/* ================================ Tab menu ================================ */
function tabname_to_hash(tabname) {
	return tabname.toLowerCase().replace(/ /g, '_');
}
function tabmenu(attacher, tabs) {
	var res = $('<div>', {class: 'tabmenu'});
	/*const*/ var prior_hash = url_hash_parser.parsed_prefix();

	for (var i = 0; i < tabs.length; i += 2)
		res.append($('<a>', {
			text: tabs[i],
			click: function(handler) {
				return function() {
					if (!$(this).hasClass('active')) {
						$(this).parent().css('min-width', $(this).parent().width());
						$(this).parent().children('.active').removeClass('active');
						$(this).addClass('active');

						window.location.hash = prior_hash + '#' + tabname_to_hash($(this).text());
						url_hash_parser.assign_as_parsed(window.location.hash);

						handler.call(this);
						centerize_modal($(this).parents('.modal'), false);
					}
				}
			}(tabs[i + 1])
		}))

	attacher(res);


	var arg = url_hash_parser.extract_next_arg();
	var rc = res.children();
	for (var i = 0; i < rc.length; ++i) {
		var elem = $(rc[i]);
		if (tabname_to_hash(elem.text()) === arg) {
			elem.parent().css('min-width', elem.parent().width());
			elem.addClass('active');

			tabs[i << 1 | 1].call(elem);
			centerize_modal(elem.parents('.modal'), false);
			return;
		}
	}

	rc.first().click();
}

/* ================================ Preview ================================ */
function preview_base(as_modal, new_window_location, func, no_modal_elem) {
	if (as_modal) {
		var elem = $('<div>', {
			class: 'modal preview',
			'prev-url': window.location.href,
			html: $('<div>', {
				html: $('<span>', { class: 'close'})
				.add('<div>', {style: 'display:block'})
			})
		}).appendTo('body');
		func.call(elem.find('div > div'));
		centerize_modal(elem);

	} else if (no_modal_elem === undefined)
		func.call($(document.body));
	else
		func.call($(no_modal_elem));

	window.history.replaceState({}, '', new_window_location);
	url_hash_parser.assign(window.location.hash);
}
function preview_ajax(as_modal, ajax_url, success_handler, new_window_location, no_modal_elem /*= document.body*/) {
	preview_base(as_modal, new_window_location, function() {
		var elem = $(this);
		API_call(ajax_url, function () {
			success_handler.apply(elem, arguments);
			if (as_modal)
				centerize_modal(elem.parent().parent());
		}, elem);
	}, no_modal_elem);
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
		if (modal_parent.length === 1) {
			function scres_handler() {
				if (obj.elem.closest('body').length === 0) {
					obj.elem = null;
					$(this).off('scroll', scres_handler);
					$(this).off('resize', scres_handler);
					return;
				}

				var height = $(this).children('div').height();
				var scroll_top = $(this).scrollTop();
				if (height - $(window).height() - scroll_top <= 300)
					obj.fetch_more();
			};

			modal_parent.on('scroll', scres_handler);
			$(window).on('resize', scres_handler);

		} else {
			function scres_handler() {
				if (obj.elem.closest('body').length === 0) {
					obj.elem = null;
					$(this).off('scroll', scres_handler);
					$(this).off('resize', scres_handler);
					return;
				}

				var x = $(document);
				if (x.height() - $(window).height() - x.scrollTop() <= 300)
					obj.fetch_more();
			};

			$(document).on('scroll', scres_handler);
			$(window).on('resize', scres_handler);
		}

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
				show_error_via_loader(logs.elem, resp, status, function () {
					logs.lock = false; // only allow manual unlocking
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
	this.job = function(job_id, actions_str, problem_id, job_preview /*= false*/) {
		if (job_preview == undefined)
			job_preview = false;

		var res = [];
		if (!job_preview && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/jobs/' + job_id, 'View', 'btn-small',
				function() { preview_job(true, job_id); }));

		if (actions_str.indexOf('u') !== -1 || actions_str.indexOf('r') !== -1)
			res.push($('<div>', {
				class: 'dropmenu down',
				html: $('<a>', {
					class: 'btn-small dropmenu-toggle',
					text: 'Download'
				}).add('<ul>', {
					html: [actions_str.indexOf('r') === -1 ? '' :  $('<a>', {
							href: '/api/job/' + job_id + '/report',
							text: 'Report'
						}), actions_str.indexOf('u') === -1 ? '' : $('<a>', {
							href: '/api/job/' + job_id + '/uploaded-package',
							text: 'Uploaded package'
						})
					]
				})
			}));

		if (actions_str.indexOf('p') !== -1)
			res.push(a_preview_button('/p/' + problem_id, 'View problem',
				'btn-small green', function() { preview_problem(true, problem_id); }));

		if (actions_str.indexOf('C') !== -1)
			res.push($('<a>', {
				class: 'btn-small red',
				text: 'Cancel job',
				click: function() {
					var jid = job_id;
					return function() { cancel_job(jid); }
				}()
			}));

		if (actions_str.indexOf('R') !== -1)
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

	this.user = function(user_id, actions_str, user_preview /*= false*/) {
		if (user_preview === undefined)
			user_preview = false;

		var res = [];
		if (!user_preview && actions_str.indexOf('v') !== -1)
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

	this.submission = function(submission_id, actions_str, submission_type, submission_preview /*= false*/) {
		if (submission_preview === undefined)
			submission_preview = false;

		var res = [];
		if (!submission_preview && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/s/' + submission_id, 'Preview', 'btn-small',
				function() { preview_submission(true, submission_id); }));

		if (actions_str.indexOf('s') !== -1) {
			if (!submission_preview)
				res.push(a_preview_button('/s/' + submission_id + '#source', 'Source',
					'btn-small', function() { preview_submission(true, submission_id, '#source'); }));

			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/submission/' + submission_id + '/download',
				text: 'Download'
			}));
		}

		if (actions_str.indexOf('C') !== -1)
			res.push(a_preview_button('/s/' + submission_id + '/chtype', 'Change type',
				'btn-small orange',
				function() { submission_chtype(submission_id, submission_type); }));

		if (actions_str.indexOf('R') !== -1)
			res.push($('<a>', {
				class: 'btn-small blue',
				text: 'Rejudge',
				click: function() { rejudge_submission(submission_id); }
			}));

		if (actions_str.indexOf('D') !== -1)
			res.push($('<a>', {
				class: 'btn-small red',
				text: 'Delete',
				click: function() { delete_submission(submission_id); }
			}));

		return res;
	}

	this.problem = function(problem_id, actions_str, problem_name, problem_preview /*= false*/) {
		if (problem_preview === undefined)
			problem_preview = false;

		var res = [];
		if (!problem_preview && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/p/' + problem_id, 'View', 'btn-small',
				function() { preview_problem(true, problem_id); }));

		if (actions_str.indexOf('V') !== -1)
			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/problem/' + problem_id + '/statement/' + encodeURIComponent(problem_name),
				text: 'Statement'
			}));

		if (actions_str.indexOf('S') !== -1)
			res.push(a_preview_button('/p/' + problem_id + '/submit', 'Submit',
				'btn-small blue', function() {
					add_submission(true, problem_id, problem_name, (actions_str.indexOf('i') !== -1));
				}));

		if (actions_str.indexOf('s') !== -1)
			res.push(a_preview_button('/p/' + problem_id + '#all_submissions#solutions',
				'Solutions', 'btn-small', function() {
					preview_problem(true, problem_id, '#all_submissions#solutions');
				}));

		if (problem_preview && actions_str.indexOf('d') !== -1)
			res.push($('<a>', {
				class: 'btn-small',
				href: '/api/problem/' + problem_id + '/download',
				text: 'Download'
			}));

		if (actions_str.indexOf('E') !== -1)
			res.push(a_preview_button('/p/' + problem_id + '/edit', 'Edit',
				'btn-small blue', function() { edit_problem(true, problem_id); }));

		if (problem_preview && actions_str.indexOf('R') !== -1)
			res.push(a_preview_button('/p/' + problem_id + '/reupload', 'Reupload',
				'btn-small orange', function() { reupload_problem(true, problem_id); }));

		if (problem_preview && actions_str.indexOf('J') !== -1)
			res.push($('<a>', {
				class: 'btn-small blue',
				text: 'Rejudge all submissions',
				click: function() { rejudge_problem_submissions(problem_id); }
			}));

		if (problem_preview && actions_str.indexOf('D') !== -1)
			res.push(a_preview_button('/p/' + problem_id + '/delete', 'Delete',
				'btn-small red', function() { delete_problem(true, problem_id); }));

		return res;
	}

	this.contest = function(contest_id, actions_str, contest_preview /*= false*/) {
		if (contest_preview === undefined)
			contest_preview = false;

		var res = [];
		if (!contest_preview && actions_str.indexOf('v') !== -1)
			res.push(a_preview_button('/c/' + contest_id, 'View', 'btn-small',
				function() { preview_contest(true, contest_id); }));

		if (contest_preview && actions_str.indexOf('E') !== -1)
			res.push(a_preview_button('/c/' + contest_id + '/edit', 'Edit',
				'btn-small blue', function() { edit_contest(true, contest_id); }));

		if (contest_preview && actions_str.indexOf('D') !== -1)
			res.push(a_preview_button('/c/' + contest_id + '/delete', 'Delete',
				'btn-small red', function() { delete_contest(true, contest_id); }));

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
function preview_user(as_modal, user_id, opt_hash /*= ''*/) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

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
				html: ActionsToHTML.user(user_id, data[6], true)
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

		var main = $(this);

		tabmenu(function(elem) { elem.appendTo(main); }, [
			'Submissions', function() {
				$(this).parent().next().remove();
				main.append($('<div>', {html: "<h2>User's submissions</h2>"}));
				tab_submissions_lister(main.children().last(), '/u' + user_id);
			},
			'Jobs', function() {
				$(this).parent().next().remove();
				var j_table = $('<table>', {
					class: 'jobs'
				});
				main.append($('<div>', {
					html: ["<h2>User's jobs</h2>", j_table]
				}));
				new JobsLister(j_table, '/u' + user_id).monitor_scroll();
			}
		]);

	}, '/u/' + user_id + (opt_hash === undefined ? '' : opt_hash));
}
function edit_user(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

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

	}, '/u/' + user_id + '/edit');
}
function delete_user(as_modal, user_id) {
	preview_ajax(as_modal, '/api/users/=' + user_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

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
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

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
function UsersLister(elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	Lister.call(this, elem);
	this.query_url = '/api/users' + query_suffix;
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: obj.query_url + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (obj.elem.children('thead').length === 0) {
					if (data.length == 0) {
						obj.elem.parent().append($('<center>', {
							class: 'users',
							html: '<p>There are no users to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							'<th class="username">Username</th>' +
							'<th class="first-name">First name</th>' +
							'<th class="last-name">Last name</th>' +
							'<th class="email">Email</th>' +
							'<th class="type">Type</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
				}

				for (var x in data) {
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
function tab_users_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.users, .loader, .loader-info').remove();
		var table = $('<table class="users stripped"></table>').appendTo(parent_elem);
		new UsersLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', function() { retab(''); },
		'Admins', function() { retab('/tA'); },
		'Teachers', function() { retab('/tT'); },
		'Normal', function() { retab('/tN'); }
	];

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}

/* ================================== Jobs ================================== */
function preview_job(as_modal, job_id, opt_hash /*= ''*/) {
	preview_ajax(as_modal, '/api/jobs/=' + job_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		function info_html(info) {
			var td = $('<td>', {
				style: 'text-align:left'
			});
			for (var name in info) {
				td.append($('<label>', {text: name}));

				if (name == "submission")
					td.append(a_preview_button('/s/' + info[name], info[name],
						undefined, function() {
							var sid = info[name];
							return function() {preview_submission(true, sid);};
						}()));
				else if (name == "problem")
					td.append(a_preview_button('/p/' + info[name], info[name],
						undefined, function() {
							var pid = info[name];
							return function() {preview_problem(true, pid);};
						}()));
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
					html: ActionsToHTML.job(job_id, data[8], data[7].problem, true)
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
		}))

		if (data[8].indexOf('r') !== -1) {
			this.append('<h2>Report preview</h2>')
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
		}
	}, '/jobs/' + job_id + (opt_hash === undefined ? '' : opt_hash));
}
function cancel_job(job_id) {
	modal_request('Canceling job ' + job_id, $('<form>'),
		'/api/job/' + job_id + '/cancel', 'The job has been canceled.');
}
function restart_job(job_id) {
	dialogue_modal_request('Restart job', $('<label>', {
			html: [
				'Are you sure to restart the ',
				a_preview_button('/jobs/' + job_id, 'job ' + job_id, undefined,
					function() { preview_job(true, job_id); }),
				'?'
			]
		}), 'Restart job', 'btn-small orange', '/api/job/' + job_id + '/restart',
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
				if (obj.elem.children('thead').length === 0) {
					if (data.length == 0) {
						obj.elem.parent().append($('<center>', {
							class: 'jobs',
							html: '<p>There are no jobs to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

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

				for (var x in data) {
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
									return function() { preview_job(true, job_id); };
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
										var sid = info.submission;
										return function() { preview_submission(true, sid); };
									}()));

						if (info.problem !== undefined)
							append_tag('problem',
								a_preview_button('/p/' + info.problem,
									info.problem, undefined, function() {
										var pid = info.problem;
										return function() { preview_problem(true, pid); };
									}()));

						var names = ['name', 'memory limit', 'problem type'];
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
				centerize_modal(obj.elem.parents('.modal'), false);

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
function tab_jobs_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.jobs, .loader, .loader-info').remove();
		var table = $('<table class="jobs"></table>').appendTo(parent_elem);
		new JobsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', function() { retab(''); },
		'My', function() { retab('/u' + logged_user_id()); }
	];

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}

/* ============================== Submissions ============================== */
function add_submission(as_modal, problem_id, problem_name_to_show, maybe_ignored, contest_problem_id) {
	preview_base(as_modal, (contest_problem_id === undefined ?
		'/p/' + problem_id + '/submit' : '/todo'), function() {
		this.append(ajax_form('Submit a solution', '/api/submission/add/p'
				+ problem_id + (contest_problem_id === undefined ? '' : '/cp' + contest_problem_id),
			$('<div>', {
				class: 'field-group',
				html: [
					$('<label>', {text: 'Problem'}),
					a_preview_button('/p/' + problem_id, problem_name_to_show, undefined,
						function() { preview_problem(true, problem_id); })
				]
			}).add(Form.field_group("Solution", {
					type: 'file',
					name: 'solution',
				})).add($('<div>', {
				class: 'field-group',
				html: [
					$('<label>', {text: 'Code'}),
					$('<textarea>', {
						class: 'monospace',
						name: 'code',
						rows: 8,
						cols: 50
					})
				]
			})).add(maybe_ignored ? Form.field_group('Ignored submission', {
				type: 'checkbox',
				name: 'ignored'
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_submission(as_modal, resp);
				} else {
					this.parent().remove();
					window.location.href = '/s/' + resp;
				}
			})
		);
	})
}
function rejudge_submission(submission_id) {
	modal_request('Scheduling submission rejudge ' + submission_id, $('<form>'),
		'/api/submission/' + submission_id + '/rejudge',
		'The rejudge has been scheduled.');
}
function submission_chtype(submission_id, submission_type) {
	dialogue_modal_request('Change submission type', $('<form>', {
		html: [
			$('<label>', {
				html: [
					'New type of the ',
					a_preview_button('/s/' + submission_id, 'submission ' + submission_id,
						undefined,
						function() { preview_submission(true, submission_id); }),
					': '
				]
			}),
			$('<select>', {
				name: 'type',
				html: [
					$('<option>', {
						value: 'N',
						text: 'Normal / final',
						selected: (submission_type !== 'Ignored' ? true : undefined)
					}),
					$('<option>', {
						value: 'I',
						text: 'Ignored',
						selected: (submission_type === 'Ignored' ? true : undefined)
					})
				]
			})
		]
	}), 'Change type', 'btn-small orange', '/api/submission/' + submission_id + '/chtype',
		'Submission type has been updated.', 'No, go back');
}
function delete_submission(submission_id) {
	dialogue_modal_request('Delete submission', $('<label>', {
			html: [
				'Are you sure to delete the ',
				a_preview_button('/s/' + submission_id, 'submission ' + submission_id,
					undefined,
					function() { preview_submission(true, submission_id); }),
				'?'
			]
		}), 'Yes, delete it', 'btn-small red', '/api/submission/' + submission_id + '/delete',
		'The submission has been deleted.', 'No, go back');
}
function preview_submission(as_modal, submission_id, opt_hash /*= ''*/) {
	preview_ajax(as_modal, '/api/submissions/=' + submission_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];
		var actions = data[15];

		if (data[6] !== null)
			this.append($('<div>', {
				class: 'round-path',
				html: [
					a_preview_button('/c/' + data[10], data[11], '', function() {
						preview_contest(true, data[10]);
					}),
					' / ',
					a_preview_button('/c/r' + data[8], data[9], '', function() {
						preview_contest(true, data[8]);
					}),
					' / ',
					a_preview_button('/c/p' + data[6], data[7], '', function() {
						preview_contest(true, data[6]);
					}),
					$('<a>', {
						class: 'btn-small',
						href: '/api/contest/p' + data[6] + '/statement/' + encodeURIComponent(data[7]),
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
							html: ActionsToHTML.submission(submission_id, actions,
								data[1], true)
						})
					]
				}),
				$('<table>', {
					html: [
						$('<thead>', {html: '<tr>' +
							(data[2] === null ? ''
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
								class: (data[1] === 'Ignored' ? 'ignored' : undefined),
								html: [
									(data[2] === null ? '' : $('<td>', {
										html: [a_preview_button('/u/' + data[2], data[3],
												undefined, function() {
													preview_user(true, data[2]);
												}),
											' (' + text_to_safe_html(data[16]) + ' ' +
												text_to_safe_html(data[17]) + ')'
										]
									})),
									$('<td>', {
										html: a_preview_button('/p/' + data[4], data[5],
											undefined, function() {
												preview_problem(true, data[4]);
											})
									}),
									normalize_datetime($('<td>', {
										datetime: data[12],
										text: data[12]
									}), true),
									$('<td>', {
										class: 'status ' + data[13][0],
										text: (data[13][0].lastIndexOf('initial') === -1
											? '' : 'Initial: ') + data[13][1]
									}),
									$('<td>', {text: data[14]}),
									$('<td>', {text: data[1]})

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
				elem.children('.tabmenu').nextAll().remove();
				elem.append($('<div>', {
					class: 'results',
					html: [data[18], data[19], null]
				}))
			}
		];

		if (actions.indexOf('s') !== -1)
			tabs.push('Source', function() {
				elem.children('.tabmenu').nextAll().remove();
				append_loader(elem);
				$.ajax({
					url: '/api/submission/' + submission_id + '/source',
					dataType: 'html',
					success: function(data) {
						elem.append(data);
						remove_loader(elem);
						centerize_modal(elem.closest('.modal'), false);
					},
					error: function(resp, status) {
						show_error_via_loader(elem, resp, status);
					}
				});
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				elem.children('.tabmenu').nextAll().remove();
				elem.append($('<table>', {class: 'jobs'}));
				new JobsLister(elem.children().last(), '/s' + submission_id).monitor_scroll();
			});

		tabmenu(function(x) { x.appendTo(elem); }, tabs);

	}, '/s/' + submission_id + (opt_hash === undefined ? '' : opt_hash));
}
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
				if (obj.elem.children('thead').length === 0) {
					if (data.length === 0) {
						obj.elem.parent().append($('<center>', {
							class: 'submissions',
							html: '<p>There are no submissions to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							(obj.show_user ? '<th class="username">Username</th>' : '') +
							'<th class="time">Added</th>' +
							'<th class="problem">Problem</th>' +
							'<th class="status">Status</th>' +
							'<th class="score">Score</th>' +
							'<th class="type">Type</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
					add_tz_marker(elem.find('thead th.time'));
				}

				for (var x in data) {
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
							html: [(obj.show_contest ? a_preview_button('/c/' + x[10], x[11], '', function() {
									var cid = x[10];
									return function() { preview_contest(true, cid); };
								}()) : ''),
								(obj.show_contest ? ' / ' : ''),
								a_preview_button('/c/r' + x[8], x[9], '', function() {
									var crid = x[8];
									return function() { preview_contest(true, crid); };
								}()),
								' / ',
								a_preview_button('/c/p' + x[6], x[7], '', function() {
									var cpid = x[6];
									return function() { preview_contest(true, cpid); };
								}())
							]
						}))

					// Status
					row.append($('<td>', {
						class: 'status ' + x[13][0],
						text: (x[13][0].lastIndexOf('initial') === -1 ? ''
							: 'Initial: ') + x[13][1]
					}));

					// Score
					row.append($('<td>', {text: x[14]}));

					// Type
					row.append($('<td>', {text: x[1]}));

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.submission(x[0], x[15], x[1])
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(obj.elem.parent());
				centerize_modal(obj.elem.parents('.modal'), false);

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
function tab_submissions_lister(parent_elem, query_suffix /*= ''*/, show_solutions_tab /* = false*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.submissions, .loader, .loader-info').remove();
		var table = $('<table class="submissions"></table>').appendTo(parent_elem);
		new SubmissionsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', function() { retab(''); },
		'Final', function() { retab('/tF'); },
		'Ignored', function() { retab('/tI'); }
	];
	if (show_solutions_tab)
		tabs.push('Solutions', function() { retab('/tS'); })

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}

/* ================================ Problems ================================ */
function add_problem(as_modal) {
	preview_base(as_modal, '/p/add', function() {
		this.append(ajax_form('Add problem', '/api/problem/add',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			}).add(Form.field_group("Problem's label", {
				type: 'text',
				name: 'label',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile or make from name',
			})).add($('<div>', {
				class: 'field-group',
				html: $('<label>', {text: "Problem's type"})
				.add('<select>', {
					name: 'type',
					required: true,
					html: $('<option>', {
						value: 'PUB',
						text: 'Public',
					}).add('<option>', {
						value: 'PRI',
						text: 'Private',
						selected: true
					}).add('<option>', {
						value: 'CON',
						text: 'Contest only',
					})
				})
			})).add(Form.field_group('Memory limit [MB]', {
				type: 'text',
				name: 'mem_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			})).add(Form.field_group('Global time limit [s] (for each test)', {
				type: 'text',
				name: 'global_time_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'No global time limit',
			})).add(Form.field_group('Set time limits using model solution', {
				type: 'checkbox',
				name: 'relative_time_limits',
				checked: true
			})).add(Form.field_group('Ignore Simfile', {
				type: 'checkbox',
				name: 'ignore_simfile',
			})).add(Form.field_group('Seek for new tests', {
				type: 'checkbox',
				name: 'seek_for_new_tests',
				checked: true
			})).add(Form.field_group('Zipped package', {
				type: 'file',
				name: 'package',
				required: true
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}, 'add-problem')
		);
	})
}
function reupload_problem(as_modal, problem_id) {
	preview_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[7];
		if (actions.indexOf('R') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Reupload problem', '/api/problem/' + problem_id + '/reupload',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				value: data[3],
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			}).add(Form.field_group("Problem's label", {
				type: 'text',
				name: 'label',
				value: data[4],
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile or make from name',
			})).add($('<div>', {
				class: 'field-group',
				html: $('<label>', {text: "Problem's type"})
				.add('<select>', {
					name: 'type',
					required: true,
					html: $('<option>', {
						value: 'PUB',
						text: 'Public',
						selected: ('Public' == data[2] ? true : undefined)
					}).add('<option>', {
						value: 'PRI',
						text: 'Private',
						selected: ('Private' == data[2] ? true : undefined)
					}).add('<option>', {
						value: 'CON',
						text: 'Contest only',
						selected: ('Contest only' == data[2] ? true : undefined)
					})
				})
			})).add(Form.field_group('Memory limit [MB]', {
				type: 'text',
				name: 'mem_limit',
				value: data[10],
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Take from Simfile',
			})).add(Form.field_group('Global time limit [s] (for each test)', {
				type: 'text',
				name: 'global_time_limit',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'No global time limit',
			})).add(Form.field_group('Set time limits using model solution', {
				type: 'checkbox',
				name: 'relative_time_limits',
				checked: true
			})).add(Form.field_group('Ignore Simfile', {
				type: 'checkbox',
				name: 'ignore_simfile',
			})).add(Form.field_group('Seek for new tests', {
				type: 'checkbox',
				name: 'seek_for_new_tests',
				checked: true
			})).add(Form.field_group('Zipped package', {
				type: 'file',
				name: 'package',
				required: true
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Submit'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_job(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/jobs/' + resp;
				}
			}, 'add-problem')
		);
	}, '/p/' + problem_id + '/reupload')
}
function rejudge_problem_submissions(problem_id) {
	dialogue_modal_request("Rejudge all problem's submissions", $('<label>', {
			html: [
				'Are you sure to rejudge all submissions to the ',
				a_preview_button('/p/' + problem_id, 'problem ' + problem_id, undefined,
					function() { preview_problem(true, problem_id); }),
				'?'
			]
		}), 'Rejudge all', 'btn-small blue',
		'/api/problem/' + problem_id + '/rejudge_all_submissions',
		'The rejudge jobs has been scheduled.', 'No, go back', true);
}
function preview_problem(as_modal, problem_id, opt_hash /*= ''*/) {
	preview_ajax(as_modal, '/api/problems/=' + problem_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];
		var actions = data[7];

		this.append($('<div>', {
			class: 'header',
			html: $('<h1>', {
					text: data[3]
				}).add('<div>', {
					html: ActionsToHTML.problem(data[0], actions, data[3], true)
				})
		})).append($('<center>', {
			html: $('<div>', {
				class: 'problem-info',
				html: $('<div>', {
					class: 'type',
					html: $('<label>', {text: 'Type'}).add('<span>', {
						text: String(data[2]).slice(0, 1).toLowerCase() +
							String(data[2]).slice(1)
					})
				})
				.add($('<div>', {
					class: 'name',
					html: $('<label>', {text: 'Name'})
				}).append(text_to_safe_html(data[3])))
				.add($('<div>', {
					class: 'label',
					html: $('<label>', {text: 'Label'})
				}).append(text_to_safe_html(data[4])))
				.add($('<div>', {
					class: 'tags',
					html: $('<label>', {text: 'Tags'})
				}).append(text_to_safe_html('')))
			})
		}));

		if (actions.indexOf('o') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'owner',
					html: $('<label>', {text: 'Owner'}).add(
						a_preview_button('/u/' + data[5], data[6], undefined, function() { preview_user(true, data[5]); }))
				}));

		if (actions.indexOf('a') !== -1)
			$(this).find('.problem-info').append($('<div>', {
					class: 'added',
					html: $('<label>', {text: 'Added'})
				}).append(normalize_datetime($('<span>', {
					datetime: data[1],
					text: data[1]
				}), true)));

		var main = $(this);
		var tabs = [];
		if (actions.indexOf('s') !== -1)
			tabs.push('All submissions', function() {
					$(this).parent().next().remove();
					main.append($('<div>'));
					tab_submissions_lister(main.children().last(), '/p' + problem_id,
						true);
				});

		if (is_logged_in())
			tabs.push('My submissions', function() {
					$(this).parent().next().remove();
					main.append($('<div>'));
					tab_submissions_lister(main.children().last(), '/p' + problem_id + '/u' + logged_user_id());
				});

		if (actions.indexOf('f') !== -1)
			tabs.push('Simfile', function() {
				$(this).parent().next().remove();
				main.append($('<pre>', {
					class: 'simfile',
					style: 'text-align: initial',
					text: data[9]
				}));
			});

		if (actions.indexOf('j') !== -1)
			tabs.push('Related jobs', function() {
				$(this).parent().next().remove();
				var j_table = $('<table>', {
					class: 'jobs'
				});
				main.append($('<div>', {
					html: j_table
				}));
				new JobsLister(j_table, '/p' + problem_id).monitor_scroll();
			});

		tabmenu(function(x) { x.appendTo(main); }, tabs);

	}, '/p/' + problem_id + (opt_hash === undefined ? '' : opt_hash));
}
function ProblemsLister(elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	this.show_owner = logged_user_is_tearcher_or_admin();
	this.show_added = logged_user_is_tearcher_or_admin() ||
		(query_suffix.indexOf('/u') !== -1);

	Lister.call(this, elem);
	this.query_url = '/api/problems' + query_suffix;
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: obj.query_url + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (obj.elem.children('thead').length === 0) {
					if (data.length === 0) {
						obj.elem.parent().append($('<center>', {
							class: 'problems',
							html: '<p>There are no problems to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							'<th class="type">Type</th>' +
							'<th class="label">Label</th>' +
							'<th class="name_and_tags">Name and tags</th>' +
							(obj.show_owner ? '<th class="owner">Owner</th>' : '') +
							(obj.show_added ? '<th class="added">Added</th>' : '') +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
					add_tz_marker(elem.find('thead th.added'));
				}

				for (var x in data) {
					x = data[x];
					obj.query_suffix = '/<' + x[0];

					var row = $('<tr>');
					// Id
					row.append($('<td>', {text: x[0]}));
					// Type
					row.append($('<td>', {text: x[2]}));
					// Label
					row.append($('<td>', {text: x[4]}));
					// Name and tags
					row.append($('<td>', {text: x[3]}));

					// Owner
					if (obj.show_owner)
						row.append($('<td>', {
							html: x[5] === null ? '' :
								(a_preview_button('/u/' + x[5], x[6], undefined,
									function() {
										var uid = x[5];
										return function() { preview_user(true, uid); };
									}()))
						}));

					// Added
					if (obj.show_added)
						row.append(normalize_datetime($('<td>', {
							datetime: x[1],
							text: x[1]
						})));

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.problem(x[0], x[7], x[3])
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(obj.elem.parent());
				centerize_modal(obj.elem.parents('.modal'), false);

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
function tab_problems_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';


	parent_elem = $(parent_elem);
	parent_elem.append($('<h1>', {text: 'Problems'}));
	if (logged_user_is_tearcher_or_admin())
		parent_elem.append(a_preview_button('/p/add', 'Add problem', 'btn', function() {
			add_problem(true);
		}));

	function retab(tab_qsuff) {
		parent_elem.children('.problems, .loader, .loader-info').remove();
		var table = $('<table class="problems stripped"></table>').appendTo(parent_elem);
		new ProblemsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', function() { retab(''); }
	];

	if (is_logged_in())
		tabs.push('My', function() { retab('/u' + logged_user_id()); });

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
}

/* ================================ Contests ================================ */
function add_contest(as_modal) {
	preview_base(as_modal, '/c/add', function() {
		this.append(ajax_form('Add contest', '/api/contest/add',
			Form.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				size: 24,
				// maxlength: 'TODO...',
				required: true
			}).add(logged_user_is_admin() ? Form.field_group('Make public', {
				type: 'checkbox',
				name: 'public'
			}) : $()).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_contest(true, resp);
				} else {
					this.parent().remove();
					window.location.href = '/c/' + resp;
				}
			})
		);
	})
}
function add_contest_round(as_modal, contest_id) {
	preview_base(as_modal, '/c/' + contest_id + '/add_round', function() {
		this.append(ajax_form('Add round', '/api/contest/c' + contest_id + '/add_round',
			Form.field_group("Round's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				required: true
			}).add(Form.field_group('Begin time [UTC]', {
				type: 'text',
				name: 'begins',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'Set current time',
			})).add(Form.field_group('End time [UTC]', {
				type: 'text',
				name: 'ends',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'yyyy-mm-dd HH:MM:SS',
			})).add(Form.field_group('Full results time [UTC]', {
				type: 'text',
				name: 'full_results',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'yyyy-mm-dd HH:MM:SS',
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_contest(true, contest_id);
				} else {
					this.parent().remove();
					// window.location.href = '/c/r' + resp;
					window.location.href = '/c/' + contest_id;
				}
			})
		);
	})
}
function add_contest_problem(as_modal, contest_round_id) {
	preview_base(as_modal, '/c/r' + contest_round_id + '/add_problem', function() {
		this.append(ajax_form('Add round', '/api/contest/r' + contest_round_id + '/add_problem',
			Form.field_group("Problem's name", {
				type: 'text',
				name: 'name',
				size: 25,
				// maxlength: 'TODO...',
				placeholder: 'The same as in Problems',
			}).add(Form.field_group('Problem ID', {
				type: 'text',
				name: 'problem_id',
				size: 6,
				// maxlength: 'TODO...',
				required: true
			}).append(a_preview_button('/p', 'Search problems', '', function() {
				preview_base(true, '/p', function() { tab_problems_lister(this); });
			}))).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Add'
				})
			}), function(resp) {
				if (as_modal) {
					close_modal(this.parent().parent().parent().parent());
					preview_contest(true, contest_id);
				} else {
					this.parent().remove();
					window.location.href = '/c/p' + resp;
				}
			})
		);
	})
}
function edit_contest(as_modal, contest_id) {
	preview_ajax(as_modal, '/api/contests/=' + contest_id, function(data) {
		if (data.length === 0)
			return show_error_via_loader(this, {
				status: '404',
				statusText: 'Not Found'
			});

		data = data[0];

		var actions = data[4];
		if (actions.indexOf('A') === -1)
			return show_error_via_loader(this, {
					status: '403',
					statusText: 'Not Allowed'
				});

		this.append(ajax_form('Edit contest', '/api/contest/c' + contest_id + '/edit',
			Form.field_group("Contest's name", {
				type: 'text',
				name: 'name',
				value: data[1],
				size: 24,
				// maxlength: 'TODO...',
				required: true
			}).add(Form.field_group('Public', {
				type: 'checkbox',
				name: 'public',
				checked: data[2],
				disabled: (data[2] || actions.indexOf('M') !== -1 ? undefined : true)
			})).add('<div>', {
				html: $('<input>', {
					class: 'btn blue',
					type: 'submit',
					value: 'Update'
				})
			})
		));

	}, '/c/' + contest_id + '/edit');
}
function preview_contest(as_modal, contest_id, opt_hash /*= ''*/) {
	preview_ajax(as_modal, '/api/contest/c' + contest_id, function(data) {
		var contest = data[0];
		var rounds = data[1];
		var problems = data[2];
		var actions = contest[4];

		// Sort rounds by items
		rounds.sort(function(a, b) { return a[2] - b[2]; });

		// Sort problems by (round_id, item)
		problems.sort(function(a, b) {
			return (a[1] == b[1] ? a[5] - b[5] : a[1] - b[1]);
		});

		// Header
		this.append($('<h1>', {text: contest[1]}));
		var elem = $(this);
		var tabs = [
			'Dashboard', function() {
				elem.children('.tabmenu').nextAll().remove();
				var dashboard = $('<div>', {class: 'dashboard'}).appendTo(elem);
				// Contest
				dashboard.append($('<div>', {
					class: 'contest',
					html: [
						$('<span>', {text: contest[1]}),
						(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/' + contest_id + '/add_round', 'Add round', 'btn-small', function() { add_contest_round(true, contest_id); })),
						(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/' + contest_id + '/edit', 'Edit', 'btn-small blue', function() { edit_contest(true, contest_id); })),
						(actions.indexOf('D') === -1 ? '' : a_preview_button('/c/' + contest_id + '/delete', 'Delete', 'btn-small red', function() { delete_contest(true, contest_id); })),
					]
				}).appendTo($('<div>')).parent());

				// Rounds and problems
				function append_round(round) {
					dashboard.append($('<div>', {
						class: 'round',
						html: [
							$('<span>', {text: round[1]}),
							$('<label>', {text: "begins"}),
							normalize_datetime($('<span>', {
								datetime: round[4],
								text: round[4]
							}), true),
							$('<label>', {text: "ends"}),
							normalize_datetime($('<span>', (round[6] === null ?
								{text: 'never'}
								: {datetime: round[6], text: round[6]})
							), true),
							$('<label>', {text: "full results"}),
							normalize_datetime($('<span>', (round[5] === null ?
								{text: 'immediately'}
								: {datetime: round[5], text: round[5]})
							), true),
							(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/r' + round[0] + '/add_problem', 'Add problem', 'btn-small', function() { add_contest_problem(true, round[0]); })),
							(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/r' + round[0] + '/edit', 'Edit', 'btn-small blue', function() { edit_contest_round(true, round[0]); })),
							(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/r' + round[0] + '/delete', 'Delete', 'btn-small red', function() { delete_contest_round(true, round[0]); })),
						]
					}).appendTo($('<div>')).parent());
				}
				for (var i = 0; i < rounds.length; ++i) {
					var round = rounds[i];
					append_round(round);
					// Round's problems
					// Bin-search first problem
					var l = 0, r = problems.length;
					while (l < r) {
						var mid = (l + r) >> 1;
						var p = problems[mid];
						if (p[1] < round[0])
							l = mid + 1;
						else
							r = mid;
					}
					// Check whether the round has ended
					var cannot_submit = (actions.indexOf('p') === -1);
					if (round[6] !== null && actions.indexOf('A') === -1) {
						var curr_time = date_to_datetime_str(new Date());
						var end_time = normalize_datetime($('<span>', {
							datetime: round[6]
						}), false).text();
						cannot_submit |= (end_time <= curr_time);
					}
					// Append problems
					var tb = $('<tbody>');
					function append_problem(problem) {
						tb.append($('<tr>', {
							html: [
								$('<td>', {text: problem[4]}),
								$('<td>', {
									html: [
										$('<a>', {
											class: 'btn-small',
											href: '/api/contest/p' + problem[0] + '/statement/' + encodeURIComponent(problem[4]),
											text: 'Statement'
										}),
										(cannot_submit ? '' : a_preview_button('/c/p' + problem[0] + '/submit', 'Submit', 'btn-small blue', function() { add_submission(true, problem[2], problem[4], (actions.indexOf('A') !== -1), problem[0]); })),
										(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/p' + problem[0] + '/edit', 'Edit', 'btn-small blue', function() { edit_contest_problem(true, problem[0]); })),
										(actions.indexOf('A') === -1 ? '' : a_preview_button('/c/p' + problem[0] + '/delete', 'Delete', 'btn-small red', function() { delete_contest_problem(true, problem[0]); })),
									]
								}),
							]
						}));
					}
					while (l < problems.length && problems[l][1] == round[0])
						append_problem(problems[l++]);

					dashboard.append($('<table>', {
						class: 'round_problems',
						html: tb
					}));
				}
			}
		];

		if (actions.indexOf('A') !== -1)
			tabs.push('All submissions', function() {
				elem.children('.tabmenu').nextAll().remove();
				tab_submissions_lister($('<div>').appendTo(elem), '/C' + contest_id);
			});

		if (actions.indexOf('p') !== -1)
			tabs.push('My submissions', function() {
				elem.children('.tabmenu').nextAll().remove();
				tab_submissions_lister($('<div>').appendTo(elem), '/C' + contest_id + '/u' + logged_user_id());
			});

		if (actions.indexOf('v') !== -1)
			tabs.push('Ranking', function() {
				elem.children('.tabmenu').nextAll().remove();
				contest_ranking($('<div>').appendTo(elem), contest_id);
			});

		tabmenu(function(x) { x.appendTo(elem); }, tabs);

	}, '/c/' + contest_id + (opt_hash === undefined ? '' : opt_hash));
}
function contest_ranking(elem_, contest_id_) {
	var elem = elem_;
	var contest_id = contest_id_;
	API_call('/api/contest/c' + contest_id, function(cdata) {
		var contest = cdata[0];
		var rounds = cdata[1];
		var problems = cdata[2];
		var actions = contest[4];
		var is_admin = (actions.indexOf('A') !== -1);

		// Sort rounds by items
		rounds.sort(function(a, b) { return a[2] - b[2]; });
		// Map rounds (by id) to their items
		var rid_to_item = new StaticMap();
		for (var i = 0; i < rounds.length; ++i) {
			if (!is_admin) {
				// Add only those rounds that have full results visible and the ranking_exposure point has passed
				var curr_time = date_to_datetime_str(new Date());
				var full_results = normalize_datetime($('<span>', {
					datetime: rounds[i][5]
				}), false).text();
				var ranking_exposure = normalize_datetime($('<span>', {
					datetime: rounds[i][3]
				}), false).text();

				if (curr_time < full_results || curr_time < ranking_exposure)
					continue; // Do not show the round
			}

			// Show the round
			rid_to_item.add(rounds[i][0], rounds[i][2]);
		}
		rid_to_item.prepare();

		// Add round item to the every problem (and remove invalid problems -
		// the ones which don't belong to the valid rounds)
		var tmp_problems = [];
		for (var i = 0; i < problems.length; ++i) {
			var x = rid_to_item.get(problems[i][1]);
			if (x != null) {
				problems[i].push(x);
				tmp_problems.push(problems[i]);
			}
		}
		var problems = tmp_problems;

		// Sort problems by (round_item, item)
		problems.sort(function(a, b) {
			return (a[7] == b[7] ? a[5] - b[5] : a[7] - b[7]);
		});

		// Map problems (by id) to their indexes in the above array
		var problem_to_col_id = new StaticMap();
		for (var i = 0; i < problems.length; ++i)
			problem_to_col_id.add(problems[i][0], i);
		problem_to_col_id.prepare();

		API_call('/api/contest/c' + contest_id + '/ranking', function(data_) {
			var data = data_;
			if (data.length == 0)
				return elem.append($('<center>', {html: '<p>There is no one in the ranking yet...</p>'}));

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
				while (rounds[j][0] != problem[1])
					++j; // Skip rounds (that have no problems attached)

				++colspan;
				// If current round's problems end
				if (i + 1 == problems.length || problems[i + 1][1] != problem[1]) {
					tr.append($('<th>', {
						colspan: colspan,
						text: rounds[j][1]
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
					text: problems[i][3]
				}));
			thead.append(tr);

			// Add score for each user add this to the user's info
			for (var i = 0; i < data.length; ++i) {
				var submissions = data[i][2];
				var total_score = 0;
				// Count only valid problems (to fix potential discrepancies
				// between ranking submissions and the contest structure)
				for (var j = 0; j < submissions.length; ++j)
					if (problem_to_col_id.get(submissions[j][2]) != null)
						total_score += submissions[j][4];

				data[i].push(total_score);
			}

			// Sort users (and their submissions) by their score
			data.sort(function(a, b) { return b[3] - a[3]; });

			// Add rows
			var tbody = $('<tbody>');
			var prev_score = data[0][3] + 1;
			var place;
			for (var i = 0; i < data.length; ++i) {
				var user_row = data[i];
				tr = $('<tr>');
				// Place
				if (prev_score != user_row[3]) {
					place = i + 1;
					prev_score = user_row[3];
				}
				tr.append($('<td>', {text: place}));
				// User
				if (user_row[0] === null)
					tr.append($('<td>', {text: user_row[1]}));
				else {
					tr.append($('<td>', {
						html: a_preview_button('/u/' + user_row[0], user_row[1], '', function() {
								var uid = user_row[0];
								return function() { preview_user(true, uid); };
							}())
					}));
				}
				// Score
				tr.append($('<td>', {text: user_row[3]}));
				// Submissions
				var row = new Array(problems.length);
				var submissions = data[i][2];
				for (var j = 0; j < submissions.length; ++j) {
					var x = problem_to_col_id.get(submissions[j][2]);
					if (x != null) {
						if (submissions[j][0] === null)
							row[x] = $('<td>', {
								class: 'status ' + submissions[j][3][0],
								text: submissions[j][4]
							});
						else {
							row[x] = $('<td>', {
								class: 'status ' + submissions[j][3][0],
								html: a_preview_button('/s/' + submissions[j][0], submissions[j][4], '', function () {
									var sid = submissions[j][0];
									return function () { preview_submission(true, sid); };
								}())
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

			centerize_modal(elem.parents('.modal'), false);

		}, elem);


	}, elem);
}
function ContestsLister(elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	Lister.call(this, elem);
	this.query_url = '/api/contests' + query_suffix;
	this.query_suffix = '';

	this.fetch_more_impl = function() {
		var obj = this;
		$.ajax({
			type: 'GET',
			url: obj.query_url + obj.query_suffix,
			dataType: 'json',
			success: function(data) {
				if (obj.elem.children('thead').length === 0) {
					if (data.length === 0) {
						obj.elem.parent().append($('<center>', {
							class: 'contests',
							html: '<p>There are no contests to show...</p>'
						}));
						remove_loader(obj.elem.parent());
						return;
					}

					elem.html('<thead><tr>' +
							'<th>Id</th>' +
							'<th class="name">Name</th>' +
							'<th class="actions">Actions</th>' +
						'</tr></thead><tbody></tbody>');
					add_tz_marker(elem.find('thead th.added'));
				}

				for (var x in data) {
					x = data[x];
					obj.query_suffix = '/<' + x[0];

					var row = $('<tr>',	{
						class: (x[2] ? undefined : 'grayed')
					});

					// Id
					row.append($('<td>', {text: x[0]}));
					// Name
					row.append($('<td>', {
						html: a_preview_button('/c/' + x[0], x[1], '', function() {
								var contest_id = x[0];
								return function () {preview_contest(true, contest_id); }
							}())
					}));

					// Actions
					row.append($('<td>', {
						html: ActionsToHTML.contest(x[0], x[4])
					}));

					obj.elem.children('tbody').append(row);
				}

				remove_loader(obj.elem.parent());
				centerize_modal(obj.elem.parents('.modal'), false);

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
function tab_contests_lister(parent_elem, query_suffix /*= ''*/) {
	if (query_suffix === undefined)
		query_suffix = '';

	parent_elem = $(parent_elem);
	function retab(tab_qsuff) {
		parent_elem.children('.contests, .loader, .loader-info').remove();
		var table = $('<table class="contests"></table>').appendTo(parent_elem);
		new ContestsLister(table, query_suffix + tab_qsuff).monitor_scroll();
	}

	var tabs = [
		'All', function() { retab(''); }
	];

	// TODO: implement it
	// if (is_logged_in())
	// 	tabs.push('My', function() { retab('/u' + logged_user_id()); });

	tabmenu(function(elem) { elem.appendTo(parent_elem); }, tabs);
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
