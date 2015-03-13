// function f(w){w.parentNode.setAttribute('class',(w.parentNode.getAttribute('class')=='dropdown'?'dropdown-open':'dropdown'));}
window.onload = function updateClock()
{
	if(load<0) time_difference=Date.parse(Date())-(start_time += load=  window.performance.timing.domContentLoadedEventEnd - window.performance.timing.navigationStart);
	var currentTime = new Date(Date.parse(Date())-time_difference);
	var currentHours = currentTime.getHours ( );
	var currentMinutes = currentTime.getMinutes ( );
	var currentSeconds = currentTime.getSeconds ( );
	currentHours = ( currentHours < 10 ? "0" : "" ) + currentHours;
	currentMinutes = ( currentMinutes < 10 ? "0" : "" ) + currentMinutes;
	currentSeconds = ( currentSeconds < 10 ? "0" : "" ) + currentSeconds;
	// Compose the string for display
	var currentTimeString = currentHours + ":" + currentMinutes + ":" + currentSeconds;
	var loadTime = window.performance.timing.domContentLoadedEventEnd - window.performance.timing.navigationStart;
	// Update the time display
	document.getElementById("clock").innerHTML = currentTimeString;
	setTimeout(function(){updateClock()},200);
}
$(document).ready(function(){
	$('.dropdown > a').click(function(event){
		event.preventDefault();
		if($(this).parent().is('.open'))
			$(this).parent().removeClass('open');
		else
		{
			$('.dropdown.open').removeClass('open');
			$(this).parent().addClass('open');
		}
	});
	$(document).click(function(event){
		if(!$(event.target).is('.open > a, .open > a *'))
			$('.dropdown.open').removeClass('open');
	});
});
