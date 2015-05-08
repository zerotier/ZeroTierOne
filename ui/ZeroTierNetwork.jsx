var ZeroTierNetwork = React.createClass({
	getInitialState: function() {
		return {};
	},

	leaveNetwork: function(event) {
		Ajax.call({
			url: 'network/'+this.props.nwid+'?auth='+this.props.authToken,
			cache: false,
			type: 'DELETE',
			success: function(data) {
				if (this.props.onNetworkDeleted)
					this.props.onNetworkDeleted(this.props.nwid);
			}.bind(this),
			error: function(error) {
			}.bind(this)
		});
		event.preventDefault();
	},

	render: function() {
		return (
			<div className="zeroTierNetwork">
				<div className="networkInfo">
					<span className="networkId">{this.props.nwid}</span>&nbsp;
					<span className="networkName">{this.props.name}</span>
				</div>
				<div className="networkProps">
					<div className="row">
						<div className="name">Status</div>
						<div className="value">{this.props['status']}</div>
					</div>
					<div className="row">
						<div className="name">Type</div>
						<div className="value">{this.props['type']}</div>
					</div>
					<div className="row">
						<div className="name">MAC</div>
						<div className="value zeroTierAddress">{this.props['mac']}</div>
					</div>
					<div className="row">
						<div className="name">MTU</div>
						<div className="value">{this.props['mtu']}</div>
					</div>
					<div className="row">
						<div className="name">Broadcast</div>
						<div className="value">{(this.props['broadcastEnabled']) ? 'ENABLED' : 'DISABLED'}</div>
					</div>
					<div className="row">
						<div className="name">Bridging</div>
						<div className="value">{(this.props['bridge']) ? 'ACTIVE' : 'DISABLED'}</div>
					</div>
					<div className="row">
						<div className="name">Device</div>
						<div className="value">{(this.props['portDeviceName']) ? this.props['portDeviceName'] : '(none)'}</div>
					</div>
					<div className="row">
						<div className="name">Managed&nbsp;IPs</div>
						<div className="value ipList">
							{
								this.props['assignedAddresses'].map(function(ipAssignment) {
									return (
										<div key={ipAssignment} className="ipAddress">{ipAssignment}</div>
									);
								})
							}
						</div>
					</div>
				</div>
				<button type="button" className="leaveNetworkButton" onClick={this.leaveNetwork}>Leave&nbsp;Network</button>
			</div>
		);
	}
});
