import { HttpClientTestingModule } from "@angular/common/http/testing";
import { ComponentFixture, TestBed } from '@angular/core/testing';
import { ReactiveFormsModule } from '@angular/forms';

import { ToastModule } from 'ng2-toastr';
import { BsModalRef, ModalModule } from 'ngx-bootstrap/modal';

import { configureTestBed } from '../../../../../testing/unit-test-helper';
import { SharedModule } from '../../../../shared/shared.module';
import { OsdRecvSpeedModalComponent } from './osd-recv-speed-modal.component';

describe('OsdRecvSpeedModalComponent', () => {
  let component: OsdRecvSpeedModalComponent;
  let fixture: ComponentFixture<OsdRecvSpeedModalComponent>;

  configureTestBed({
    imports: [
      HttpClientTestingModule,
      ModalModule.forRoot(),
      ReactiveFormsModule,
      SharedModule,
      ToastModule.forRoot()
    ],
    declarations: [OsdRecvSpeedModalComponent],
    providers: [BsModalRef]
  });

  beforeEach(() => {
    fixture = TestBed.createComponent(OsdRecvSpeedModalComponent);
    component = fixture.componentInstance;
    fixture.detectChanges();
  });

  it('should create', () => {
    expect(component).toBeTruthy();
  });
});
