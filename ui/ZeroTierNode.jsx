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
		var nid = event.target.value;
		if (nid) {
			nid = nid.toLowerCase();
			var nnid = '';
			for(var i=0;((i<nid.length)&&(i<16));++i) {
				if ("0123456789abcdef".indexOf(nid.charAt(i)) >= 0)
					nnid += nid.charAt(i);
			}
			this.networkToJoin = nnid;
			event.target.value = nnid;
		} else {
			this.networkToJoin = '';
			event.target.value = '';
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
		this.tabIndex = 0;
		this.updateAll();
		this.updateIntervalId = setInterval(this.updateAll,2500);
	},
	componentWillUnmount: function() {
		clearInterval(this.updateIntervalId);
	},
	render: function() {
		/* We implement tabs in a very simple way here with a React JSX conditional. The tabIndex
		 * local variable indicates the tab, and switching it determines which set of things we
		 * render in the main middle portion. On tab switch calls forceUpdate(). */
		return (
			<div className="zeroTierNode">
				<div className="top">&nbsp;&nbsp;
					<button disabled={this.tabIndex === 0} onClick={function() {this.tabIndex = 0; this.forceUpdate();}.bind(this)}>Networks</button>
					<button disabled={this.tabIndex === 1} onClick={function() {this.tabIndex = 1; this.forceUpdate();}.bind(this)}>Peers</button>
					<div className="logo">&#x23c1;&nbsp;</div>
				</div>
				<div className="middle">
					<div className="middleScroll">
						{
							(this.tabIndex === 1) ? (
								<div className="peers">
									<div className="peerHeader">
										<div className="f">Address</div>
										<div className="f">Version</div>
										<div className="f">Latency</div>
										<div className="f">Data&nbsp;Paths</div>
										<div className="f">Last&nbsp;Frame</div>
										<div className="f">Role</div>
									</div>
									{
										this.state._peers.map(function(peer) {
											return (
												<div className="peer">
													<div className="f zeroTierAddress">{peer['address']}</div>
													<div className="f">{(peer['version'] === '-1.-1.-1') ? '-' : peer['version']}</div>
													<div className="f">{peer['latency']}</div>
													<div className="f">
														{
															(peer['paths'].length === 0) ? (
																<div className="peerPath"></div>
															) : (
																<div>
																{
																	peer['paths'].map(function(path) {
																		var cn = ((path.active)||(path.fixed)) ? (path.preferred ? 'peerPathPreferred' : 'peerPathActive') : 'peerPathInactive';
																		return (
																			<div className={cn}>{path.address}&nbsp;&nbsp;{this.ago(path.lastSend)}/{this.ago(path.lastReceive)}</div>
																		);
																	}.bind(this))
																}
																</div>
															)
														}
													</div>
													<div className="f">{this.ago(peer['lastUnicastFrame'])}</div>
													<div className="f">{peer['role']}</div>
												</div>
											);
										}.bind(this))
									}
								</div>
							) : (
								<div className="networks">
									{
										this.state._networks.map(function(network) {
											network['authToken'] = this.props.authToken;
											network['onNetworkDeleted'] = this.handleNetworkDelete;
											return React.createElement('div',{className: 'network'},React.createElement(ZeroTierNetwork,network));
										}.bind(this))
									}
								</div>
							)
						}
					</div>
				</div>
				<div className="bottom">
					<div className="left">
						<span className="statusLine"><span className="zeroTierAddress">{this.state.address}</span>&nbsp;&nbsp;{this.state.online ? 'ONLINE' : 'OFFLINE'}&nbsp;&nbsp;{this.state.version}</span>
					</div>
					<div className="right">
						<form onSubmit={this.joinNetwork}><input type="text" placeholder="  [ Network ID ]" onChange={this.handleNetworkIdEntry} size="16"/><button type="submit">Join</button></form>
					</div>
				</div>
			</div>
		);
	}
});
