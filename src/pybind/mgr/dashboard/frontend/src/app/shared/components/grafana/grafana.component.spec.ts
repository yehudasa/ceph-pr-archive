import { HttpClientTestingModule } from '@angular/common/http/testing';
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { RouterTestingModule } from '@angular/router/testing';

import { AlertModule } from 'ngx-bootstrap/alert';

import { configureTestBed } from '../../../../testing/unit-test-helper';
import { GrafanaService } from '../../../shared/api/grafana.service';
import { SummaryService } from '../../../shared/services/summary.service';
import { CephReleaseNamePipe } from '../../pipes/ceph-release-name.pipe';
import { InfoPanelComponent } from '../info-panel/info-panel.component';
import { LoadingPanelComponent } from '../loading-panel/loading-panel.component';
import { GrafanaComponent } from './grafana.component';

describe('GrafanaComponent', () => {
  let component: GrafanaComponent;
  let fixture: ComponentFixture<GrafanaComponent>;

  configureTestBed({
    declarations: [GrafanaComponent, InfoPanelComponent, LoadingPanelComponent],
    imports: [AlertModule.forRoot(), HttpClientTestingModule, RouterTestingModule],
    providers: [CephReleaseNamePipe, GrafanaService, SummaryService]
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(GrafanaComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
