var ZeroTierNode = React.createClass({
	getInitialState: function() {
		return {
			address: '----------',
			online: false,
			version: '_._._',
			_networks: [],
			_peers: []
		};
	},

	ago: function(ms) {
		if (ms > 0) {
			var tmp = Math.round((Date.now() - ms) / 1000);
			return ((tmp > 0) ? tmp : 0);
		} else return 0;
	},

	updatePeers: function() {
		Ajax.call({
			url: 'peer?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data) {
					var pl = JSON.parse(data);
					if (Array.isArray(pl)) {
						this.setState({_peers: pl});
					}
				}
			}.bind(this),
			error: function() {
			}.bind(this)
		});
	},
	updateNetworks: function() {
		Ajax.call({
			url: 'network?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				if (data) {
					var nwl = JSON.parse(data);
					if (Array.isArray(nwl)) {
						this.setState({_networks: nwl});
					}
				}
			}.bind(this),
			error: function() {
			}.bind(this)
		});
	},
	updateAll: function() {
		Ajax.call({
			url: 'status?auth='+this.props.authToken,
			cache: false,
			type: 'GET',
			success: function(data) {
				this.alertedToFailure = false;
				if (data) {
					var status = JSON.parse(data);
					this.setState(status);
					document.title = 'ZeroTier One [' + status.address + ']';
				}
				this.updateNetworks();
				this.updatePeers();
			}.bind(this),
			error: function() {
				this.setState(this.getInitialState());
				if (!this.alertedToFailure) {
					this.alertedToFailure = true;
					alert('Authorization token invalid or ZeroTier One service not running.');
				}
			}.bind(this)
		});
	},
	joinNetwork: function(event) {
		event.preventDefault();
		if ((this.networkToJoin)&&(this.networkToJoin.length === 16)) {
			Ajax.call({
				url: 'network/'+this.networkToJoin+'?auth='+this.props.authToken,
				cache: false,
				type: 'POST',
				success: function(data) {
					this.networkToJoin = '';
					if (this.networkInputElement)
						this.networkInputElement.value = '';
					this.updateNetworks();
				}.bind(this),
				error: function() {
				}.bind(this)
			});
		} else {
			alert('To join a network, enter its 16-digit network ID.');
		}
	},
	handleNetworkIdEntry: function(event) {
		this.networkInputElement = event.target;
		var nid = this.networkInputElement.value;
		if (nid) {
			nid = nid.toLowerCase();
			var nnid = '';
			for(var i=0;((i<nid.length)&&(i<16));++i) {
				if ("0123456789abcdef".indexOf(nid.charAt(i)) >= 0)
					nnid += nid.charAt(i);
			}
			this.networkToJoin = nnid;
			this.networkInputElement.value = nnid;
		} else {
			this.networkToJoin = '';
			this.networkInputElement.value = '';
		}
	},

	handleNetworkDelete: function(nwid) {
		var networks = [];
		for(var i=0;i<this.state._networks.length;++i) {
			if (this.state._networks[i].nwid !== nwid)
				networks.push(this.state._networks[i]);
		}
		this.setState({_networks: networks});
	},

	componentDidMount: function() {
		this.updateAll();
		this.updateIntervalId = setInterval(this.updateAll,2500);
	},
	componentWillUnmount: function() {
		clearInterval(this.updateIntervalId);
	},
	render: function() {
		return (
			<div className="zeroTierNode">
				<div className="middle"><div className="middleCell">
					<div className="middleScroll">
						<div className="networks" key="_networks">
							{
								this.state._networks.map(function(network) {
									network['authToken'] = this.props.authToken;
									network['onNetworkDeleted'] = this.handleNetworkDelete;
									return React.createElement('div',{className: 'network',key: network.nwid},React.createElement(ZeroTierNetwork,network));
								}.bind(this))
							}
						</div>
					</div>
				</div></div>
				<div className="bottom">
					<div className="left">
						<span className="statusLine"><span className="zeroTierAddress">{this.state.address}</span>&nbsp;&nbsp;{this.state.online ? (this.state.tcpFallbackActive ? 'TUNNELED' : 'ONLINE') : 'OFFLINE'}&nbsp;&nbsp;{this.state.version}</span>
					</div>
					<div className="right">
						<form onSubmit={this.joinNetwork}><input type="text" maxlength="16" placeholder="[ Network ID ]" onChange={this.handleNetworkIdEntry} size="16"/><button type="button" onClick={this.joinNetwork}>Join</button></form>
					</div>
				</div>
			</div>
		);
	}
});
