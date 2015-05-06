var ZeroTierNode = React.createClass({
	getInitialState: function() {
		return {
			address: '----------',
			online: false,
			version: '_._._'
		};
	},

	updateAll: function() {
		Ajax.call({
			url: 'status?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data)
					this.setState(JSON.parse(data));
			}.bind(this),
			error: function() {
				this.setState(this.getInitialState());
			}.bind(this)
		})
	},

	componentDidMount: function() {
		this.updateAll();
//		this.updateIntervalId = setInterval(this.updateAll,2500);
	},
	componentWillUnmount: function() {
//		clearInterval(this.updateIntervalId);
	},
	render: function() {
		return (
			<div className="container-fluid zeroTierNode">
				<div className="row">
				</div>
				<div className="row">
					<div className="col-xs-8">
						<span className="zerotier-address">{this.state.address}</span>
						<span className="zerotier-node-statusline">{this.state.online ? 'ONLINE' : 'OFFLINE'}&nbsp;&nbsp;{this.state.version}</span>
					</div>
					<div className="col-xs-4">
						<form>
							<input type="text"/>
						</form>
					</div>
				</div>
			</div>
		);
	}
});
