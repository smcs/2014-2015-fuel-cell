
/*
 * GET home page.
 */

exports.index = function(req, res){
//    res.header('Access-Control-Allow-Origin', req.headers.origin);
    res.render('index', { title: 'What is Matt Doing?' });
};

exports.eventlog = function () {
    return function (req, res) {
        res.render('eventlog', { title: 'Event Log' });
    };
};
