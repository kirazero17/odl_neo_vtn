/*
 * Copyright (c) 2012-2013 NEC Corporation
 * All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this
 * distribution, and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#include "PhysicalLayerStub.hh"
using namespace unc::uppl;
void PhysicalLayerStub :: loadphysicallayer() {

   pfc_modattr_t* modattr;
   PhysicalLayer = new unc::uppl::PhysicalLayer(modattr);
}

IPCConnectionManager* PhysicalLayerStub::get_ipc_connection_manager() {
  return new IPCConnectionManager;
}

void PhysicalLayerStub :: unloadphysicallayer() {

   PhysicalLayer = NULL;
}
