import { Component, Input, OnChanges } from '@angular/core';

import { CdTableSelection } from '../../../../shared/models/cd-table-selection';
import { Permission } from '../../../../shared/models/permissions';
import { AuthStorageService } from '../../../../shared/services/auth-storage.service';

@Component({
  selector: 'cd-host-details',
  templateUrl: './host-details.component.html',
  styleUrls: ['./host-details.component.scss']
})
export class HostDetailsComponent implements OnChanges {
  grafanaScope: Permission;
  @Input()
  selection: CdTableSelection;
  host: any;

  constructor(private authStorageService: AuthStorageService) {
    this.grafanaScope = this.authStorageService.getPermissions().grafana;
  }

  ngOnChanges() {
    if (this.selection.hasSelection) {
      this.host = this.selection.first();
    }
  }
}
