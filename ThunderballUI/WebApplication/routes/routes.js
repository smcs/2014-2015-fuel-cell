// Route file for rendering views


// Main page
exports.jadeindex = function (req, res) {
    res.render('jadeindex');
};

exports.summary = function (req, res) {
    res.render('summary', { title: 'Fuel Cell Status' });
};

// Event Log
exports.eventlog = function (req, res) {
    res.render('eventlog', { title: 'Event Log' });
};

// Stats page
exports.statsview = function(req, res){
    //    res.header('Access-Control-Allow-Origin', req.headers.origin);
    res.render('statsview', { title: 'Fuel Cell Statistics' });
};
