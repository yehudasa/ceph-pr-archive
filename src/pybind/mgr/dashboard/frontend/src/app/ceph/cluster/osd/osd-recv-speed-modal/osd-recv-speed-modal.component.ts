import { Component, OnInit } from '@angular/core';
import { FormControl } from '@angular/forms';
import { BsModalRef } from 'ngx-bootstrap/modal';

import * as _ from 'lodash';
import { forkJoin as observableForkJoin, of } from 'rxjs';
import { mergeMap } from 'rxjs/internal/operators';

import { ConfigurationService } from '../../../../shared/api/configuration.service';
import { NotificationType } from '../../../../shared/enum/notification-type.enum';
import { CdFormGroup } from '../../../../shared/forms/cd-form-group';
import { NotificationService } from '../../../../shared/services/notification.service';
import { OsdRecvSpeedProfiles } from './osd-recv-speed-modal.profiles';

@Component({
  selector: 'cd-osd-recv-speed-modal',
  templateUrl: './osd-recv-speed-modal.component.html',
  styleUrls: ['./osd-recv-speed-modal.component.scss']
})
export class OsdRecvSpeedModalComponent implements OnInit {
  osdRecvSpeedForm: CdFormGroup;
  profiles = OsdRecvSpeedProfiles.KNOWN_PROFILES;
  profileAttrs = [
    'osd_max_backfills',
    'osd_recovery_max_active',
    'osd_recovery_max_single_start',
    'osd_recovery_sleep'
  ];

  constructor(
    public bsModalRef: BsModalRef,
    private configService: ConfigurationService,
    private notificationService: NotificationService
  ) {
    this.osdRecvSpeedForm = new CdFormGroup({
      profile: new FormControl(null, {}),
      customizeProfile: new FormControl(false)
    });
    this.profileAttrs.forEach((attr) => {
      this.osdRecvSpeedForm.addControl(attr, new FormControl(null));
    });
  }

  ngOnInit() {
    this.setActiveProfile();
  }

  setProfileValues(profileName: string, profileValues: any) {
    this.osdRecvSpeedForm.controls.profile.setValue(profileName);
    Object.entries(profileValues).forEach(([name, value]) => {
      this.osdRecvSpeedForm.controls[name].setValue(value);
    });
  }

  onCustomizeProfileChange() {
    console.log('onCustomizeProfileChange');
  }

  setActiveProfile() {
    const observables = [];
    this.profileAttrs.forEach((configName) => {
      observables.push(this.configService.get(configName));
    });

    observableForkJoin(observables)
      .pipe(
        mergeMap((configOptions) => {
          const result = {};
          configOptions.forEach((configOption) => {
            if ('value' in configOption) {
              configOption.value.forEach((value) => {
                if (value['section'] === 'osd') {
                  result[configOption.name] = Number(value.value);
                }
              });
            }
          });
          return of(result);
        })
      )
      .subscribe((resp) => {
        let profile =
          _.find(this.profiles, (p) => {
            return _.isEqual(p.values, resp);
          });

        if (!profile) {
          console.log(resp);
          profile = this.profiles[0];
        }

        this.setProfileValues(profile.name, profile.values);
      });
  }

  onProfileChange(selectedProfileName) {
    const selectedProfile = _.find(this.profiles, (p) => {
      return p.name === selectedProfileName;
    });
    this.setProfileValues(selectedProfile.name, selectedProfile.values);
  }

  submitAction() {
    const options = {};
    this.profileAttrs.forEach((attr) => {
      options[attr] = { section: 'osd', value: this.osdRecvSpeedForm.getValue(attr) };
    });

    this.configService.bulkCreate({ options: options }).subscribe(
      () => {
        this.notificationService.show(
          NotificationType.success,
          'OSD recovery speed profile "' + this.osdRecvSpeedForm.getValue('profile') + '" was set successfully.',
          'OSD recovery speed profile'
        );
        this.bsModalRef.hide();
      },
      () => {
        this.bsModalRef.hide();
      }
    );
  }
}
